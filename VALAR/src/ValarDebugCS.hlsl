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

#define VRS_RootSig \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants=6), " \
    "DescriptorTable(UAV(u0, numDescriptors = 2))," \

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

cbuffer CB0 : register(b0) {
    uint NativeWidth;
    uint NativeHeight;
   
    uint UpscaledWidth;
    uint UpscaledHeight;

    uint ShadingRateTileSize;
    bool DrawGrid;
}

RWTexture2D<uint> VRSShadingRateBuffer : register(u0);
uint GetShadingRate(int2 st) { return VRSShadingRateBuffer[st]; }

RWTexture2D<float4> ColorBuffer : register(u1);
void SetColor(int2 st, float4 rgb) { ColorBuffer[st] = rgb; }

bool IsTileEdge(uint2 PixelCoord)
{
    bool isTileEdge = false;

    if (PixelCoord.x % ShadingRateTileSize == 0)
        isTileEdge = true;
    else if (PixelCoord.y % ShadingRateTileSize == 0)
        isTileEdge = true;

    return isTileEdge;
}

bool IsIndicatorPosition(uint2 PixelCoord, uint TileSize)
{
    return ((PixelCoord.x % TileSize == 4 && PixelCoord.y % TileSize == 4) ||
        (PixelCoord.x % TileSize == 4 && PixelCoord.y % TileSize == 3) ||
        (PixelCoord.x % TileSize == 3 && PixelCoord.y % TileSize == 4) ||
        (PixelCoord.x % TileSize == 3 && PixelCoord.y % TileSize == 3));
}

[numthreads(1, 1, 1)]
[RootSignature(VRS_RootSig)]
void main(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    float2 upscaleRatio = float2(
        (float)NativeWidth / (float)UpscaledWidth,
        (float)NativeHeight / (float)UpscaledHeight);
        
    float2 scaledPixelCoord = float2(
        (float)DTid.x * upscaleRatio.x,
        (float)DTid.y * upscaleRatio.y);
        
    uint2 vrsBufferCoord = uint2(
        (uint)((float)scaledPixelCoord.x / (float)ShadingRateTileSize),
        (uint)((float)scaledPixelCoord.y / (float)ShadingRateTileSize));

    uint rate = GetShadingRate(vrsBufferCoord);

    if (DrawGrid && IsTileEdge(scaledPixelCoord))
    {
        SetColor(DTid.xy, float4(0, 0, 0, 1));
        return;
    }

    switch (rate)
    {
    case SHADING_RATE_1X1:
        break;
    case SHADING_RATE_1X2:
        if (IsIndicatorPosition(scaledPixelCoord, ShadingRateTileSize))
            SetColor(DTid.xy, float4(1, 1, 1, 1));
        else 
            SetColor(DTid.xy, float4(0, 0, 1, 1));
        break;
    case SHADING_RATE_2X1:
        SetColor(DTid.xy, float4(0, 0, 1, 1));
        break;
    case SHADING_RATE_2X2:
        SetColor(DTid.xy, float4(0, 1, 0, 1));
        break;
    case SHADING_RATE_2X4:
        if (IsIndicatorPosition(scaledPixelCoord, ShadingRateTileSize))
            SetColor(DTid.xy, float4(1, 1, 1, 1));
        else
            SetColor(DTid.xy, float4(1, 0, 0, 1));
        break;
    case SHADING_RATE_4X2:
        SetColor(DTid.xy, float4(1, 0, 0, 1));
        break;
    case SHADING_RATE_4X4:
        SetColor(DTid.xy, float4(1, 0, 1, 1));
        break;
    }
}