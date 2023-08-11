// Copyright (C) 2022 Intel Corporation

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom
// the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.

#include "VRSCommon.hlsli"

#define VRS_RootSig \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants=13), " \
    "DescriptorTable(UAV(u0, numDescriptors = 4))," \

cbuffer CB0 : register(b0) {
    uint2 TextureSize;
    uint ShadingRateTileSize;
    float SensitivityThreshold;
    float EnvLuma;
    float K;
    float WeberFechnerConstant;
    bool UseWeberFechner;
    bool UseMotionVectors;
    bool AllowQuarterRate;

    // Intel XeSS Support
    uint2 UpscaledSize;
    bool UseUpscaledMotionVectors;
}

#define USE_VELOCITY
//#define BRANCHLESS

#define H_SLOPE ((0.0468f - 1.0f) / (16.0f - 0.0f))
#define Q_SLOPE ((0.1629f - K) / (16.0f - 0.0f))
#define H_INTERCEPT 1.0f
#define Q_INTERCEPT K

RWTexture2D<float4> ColorBuffer : register(u1);
float4 FetchColor(int2 st) { return ColorBuffer[st]; }

#ifdef USE_VELOCITY
RWTexture2D<uint> VelocityBuffer : register(u2);
RWTexture2D<float2> UpscaledVelocityBuffer : register(u3);

float UnpackXY(uint x)
{
    return f16tof32((x & 0x1FF) << 4 | (x >> 9) << 15) * 32768.0;
}

float UnpackZ(uint x)
{
    return f16tof32((x & 0x7FF) << 2 | (x >> 11) << 15) * 128.0;
}

float3 UnpackVelocity(uint Velocity)
{
    return float3(UnpackXY(Velocity & 0x3FF), UnpackXY((Velocity >> 10) & 0x3FF), UnpackZ(Velocity >> 20));
}

#endif

[RootSignature(VRS_RootSig)]
[numthreads(8, 8, 1)]
void main(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint3 tileIndex = (DTid * ShadingRateTileSize) + (ShadingRateTileSize / 2.0f);

    float velocity = 0.0f;
    const float4 centroidColor1 = FetchColor(uint2(tileIndex.x, tileIndex.y));
    const float4 centroidColor2 = FetchColor(uint2(tileIndex.x + 1, tileIndex.y));
    const float4 centroidColor3 = FetchColor(uint2(tileIndex.x, tileIndex.y + 1));
    const float4 centroidColor4 = FetchColor(uint2(tileIndex.x + 1, tileIndex.y + 1));

    const float centroidLuma1 = RGBToLuminance(centroidColor1 * centroidColor1);
    const float centroidLuma2 = RGBToLuminance(centroidColor2 * centroidColor2);
    const float centroidLuma3 = RGBToLuminance(centroidColor3 * centroidColor3);
    const float centroidLuma4 = RGBToLuminance(centroidColor4 * centroidColor4);

    // Compute Average Luminance of Current Tile
    const float avgTileLuma = (centroidLuma1 + centroidLuma2 + centroidLuma3 + centroidLuma4) / 4.0f;
    const float avgTileLumaX = ((abs(centroidLuma2 - centroidLuma1)) + (abs(centroidLuma4 - centroidLuma3)));
    const float avgTileLumaY = ((abs(centroidLuma3 - centroidLuma1)) + (abs(centroidLuma4 - centroidLuma2)));

    // Satifying Equation 15 http://leiy.cc/publications/nas/nas-pacmcgit.pdf
    const float jnd_threshold = (SensitivityThreshold / 2.0f) * (avgTileLuma + EnvLuma);

    // Compute the MSE error for Luma X/Y derivatives
    const float avgErrorX = sqrt(avgTileLumaX);
    const float avgErrorY = sqrt(avgTileLumaY);

    // Motion Vector based velocity compensation.
    float velocityHError = 1.0;
    float velocityQError = K;

#ifdef USE_VELOCITY
    if (UseMotionVectors) {
        if (UseUpscaledMotionVectors) {
            const float2 upscaleRatio = float2((float)UpscaledSize.x / (float)TextureSize.x,
                (float)UpscaledSize.y / (float)TextureSize.y);

            const uint2 uPixelCoord =
                float2((float)tileIndex.x * upscaleRatio.x, (float)tileIndex.y * tileIndex.y);

            velocity = length(UpscaledVelocityBuffer[uPixelCoord].xy);
        } else {
            velocity = length(UnpackVelocity(VelocityBuffer[tileIndex.xy]));
        }
    }
#endif
#ifdef BRANCHLESS
#ifdef USE_VELOCITY
    // Satifying Equation 20. http://leiy.cc/publications/nas/nas-pacmcgit.pdf
    // velocityHError = pow(1.0 / (1.0 + pow(1.05 * minTileVelocity, 3.10)), 0.35);
    velocityHError = mad(1.0f, (float)(!UseMotionVectors),
        mad(H_SLOPE, velocity, H_INTERCEPT) * (float)UseMotionVectors);

    // Satifying Equation 21. http://leiy.cc/publications/nas/nas-pacmcgit.pdf
    // velocityQError = K * pow(1.0 / (1.0 + pow(0.55 * minTileVelocity, 2.41)), 0.49);
    velocityQError = mad(K, (float)(!UseMotionVectors),
        mad(Q_SLOPE, velocity, Q_INTERCEPT) * (float)UseMotionVectors);
#endif
    const bool fullRateCmpX = ((velocityHError * avgErrorX) >= jnd_threshold);
    const bool quarterRateCmpX = ((velocityQError * avgErrorX) < jnd_threshold);

    const bool fullRateCmpY = ((velocityHError * avgErrorY) >= jnd_threshold);
    const bool quarterRateCmpY = ((velocityQError * avgErrorY) < jnd_threshold);

    const uint rate2x = D3D12_AXIS_SHADING_RATE_2X;
    const uint rate4x = mad(D3D12_AXIS_SHADING_RATE_2X, (uint)(!AllowQuarterRate), 
                           (D3D12_AXIS_SHADING_RATE_4X * (uint)(AllowQuarterRate)));

    uint xRate = mad(
        rate2x, (int)(!(fullRateCmpX || quarterRateCmpX)), (uint)(rate4x * quarterRateCmpX));

    uint yRate = mad(
        rate2x, (uint)(!(fullRateCmpY || quarterRateCmpY)), (uint)(rate4x * quarterRateCmpY));
#else
#ifdef USE_VELOCITY
    if (UseMotionVectors) {
        // Satifying Equation 20. http://leiy.cc/publications/nas/nas-pacmcgit.pdf
        // velocityHError = pow(1.0 / (1.0 + pow(1.05 * minTileVelocity, 3.10)), 0.35);
        velocityHError = mad(H_SLOPE, velocity, H_INTERCEPT);

        // Satifying Equation 21. http://leiy.cc/publications/nas/nas-pacmcgit.pdf
        // velocityQError = K * pow(1.0 / (1.0 + pow(0.55 * minTileVelocity, 2.41)), 0.49);
        velocityQError = K * mad(Q_SLOPE, velocity, Q_INTERCEPT);
    }
#endif

    uint xRate = D3D12_AXIS_SHADING_RATE_2X;
    uint yRate = D3D12_AXIS_SHADING_RATE_2X;

    // Satifying Equation 16. For X http://leiy.cc/publications/nas/nas-pacmcgit.pdf
    if ((velocityHError * avgErrorX) >= jnd_threshold) {
        xRate = D3D12_AXIS_SHADING_RATE_1X;
    }
    // Satifying Equation 14. For Qurter Rate Shading for X
    // http://leiy.cc/publications/nas/nas-pacmcgit.pdf
    /*else if ((velocityQError * avgErrorX) < jnd_threshold) {
        if (AllowQuarterRate) {
            xRate = D3D12_AXIS_SHADING_RATE_4X;
        }
    }*/

    // Satifying Equation 16 For Y http://leiy.cc/publications/nas/nas-pacmcgit.pdf
    if ((velocityHError * avgErrorY) >= jnd_threshold) {
        // Logic to Prevent Invalid 4x1 shading Rate, converts to 4X2.
        yRate = (xRate == D3D12_AXIS_SHADING_RATE_4X ? D3D12_AXIS_SHADING_RATE_2X
            : D3D12_AXIS_SHADING_RATE_1X);
    }
    // Satifying Equation 14 For Qurter Rate Shading for Y
    // http://leiy.cc/publications/nas/nas-pacmcgit.pdf
    //else if ((velocityQError * avgErrorY) < jnd_threshold) {
    //    if (AllowQuarterRate) {
    //        // Logic to Prevent 4x4 Shading Rate and invalid 1X4, converts to 4x2 or 1x2.
    //        yRate = (xRate == D3D12_AXIS_SHADING_RATE_1X ? D3D12_AXIS_SHADING_RATE_2X
    //            : D3D12_AXIS_SHADING_RATE_4X);
    //    }
    //}
#endif
    if (yRate == D3D12_AXIS_SHADING_RATE_1X && 
        xRate == D3D12_AXIS_SHADING_RATE_4X)
        xRate = D3D12_AXIS_SHADING_RATE_2X;
    else if (yRate == D3D12_AXIS_SHADING_RATE_4X && 
        xRate == D3D12_AXIS_SHADING_RATE_1X)
        yRate = D3D12_AXIS_SHADING_RATE_2X;

    SetShadingRate(DTid.xy, D3D12_MAKE_COARSE_SHADING_RATE(xRate, yRate));
}