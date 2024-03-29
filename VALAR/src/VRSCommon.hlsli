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

#define UINT16_MAX 65536
#define UINT32_MAX 4294967295
#define D3D12_SHADING_RATE_X_AXIS_SHIFT 2
#define D3D12_SHADING_RATE_VALID_MASK 3
#define D3D12_MAKE_COARSE_SHADING_RATE(x,y) ((x) << D3D12_SHADING_RATE_X_AXIS_SHIFT | (y))
#define D3D12_GET_COARSE_SHADING_RATE_X_AXIS(x) (((x) >> D3D12_SHADING_RATE_X_AXIS_SHIFT) & D3D12_SHADING_RATE_VALID_MASK)
#define D3D12_GET_COARSE_SHADING_RATE_Y_AXIS(y) ((y) & D3D12_SHADING_RATE_VALID_MASK)

RWTexture2D<uint> VRSShadingRateBuffer : register(u0);

enum ShadingRates
{
    SHADING_RATE_1X1 = 0x0,
    SHADING_RATE_1X2 = 0x1,
    SHADING_RATE_2X1 = 0x4,
    SHADING_RATE_2X2 = 0x5,
    SHADING_RATE_2X4 = 0x6,
    SHADING_RATE_4X2 = 0x9,
    SHADING_RATE_4X4 = 0xa,
};

enum ShadingRateAxis
{
    D3D12_AXIS_SHADING_RATE_1X = 0,
    D3D12_AXIS_SHADING_RATE_2X = 0x1,
    D3D12_AXIS_SHADING_RATE_4X = 0x2
};

uint GetShadingRate(int2 st)
{
    return VRSShadingRateBuffer[st];
}

void SetShadingRate(int2 st, uint rate)
{
    VRSShadingRateBuffer[st] = rate;
}

float RGBToLeiLuminance(float4 LinearRGBA)
{
    float3 rgb = float3(LinearRGBA.x, LinearRGBA.y, LinearRGBA.z);
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

float RGBToLogLuminance(float4 LinearRGBA)
{
    float3 rgb = float3(LinearRGBA.x, LinearRGBA.y, LinearRGBA.z);
    float Luma = dot(rgb, float3(0.212671, 0.715160, 0.072169));
    return log2(1 + Luma * 15) / 4;
}

float RGBToLuminance(float4 LinearRGBA)
{
    float3 rgb = float3(LinearRGBA.x, LinearRGBA.y, LinearRGBA.z);
    return dot(rgb, float3(0.212671, 0.715160, 0.072169));
    
}