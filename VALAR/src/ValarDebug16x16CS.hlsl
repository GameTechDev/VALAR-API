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
    "RootConstants(b0, num32BitConstants=10), " \
    "DescriptorTable(UAV(u0, numDescriptors = 3))," \

RWTexture2D<float4> ColorBuffer : register(u1);
void SetColor(int2 st, float4 color) { ColorBuffer[st] = color; }

[numthreads(16, 16, 1)]
[RootSignature(VRS_RootSig)]
void main(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint rate = GetShadingRate(Gid.xy);

    switch (rate)
    {
    case SHADING_RATE_1X1:
        break;
    case SHADING_RATE_1X2:
        if (GTid.x == 1 && GTid.y == 1) SetColor(DTid.xy, float4(1, 1, 1, 1));
        if (GTid.x == 1 && GTid.y == 2) SetColor(DTid.xy, float4(1, 1, 1, 1));
        if (GTid.x == 2 && GTid.y == 1) SetColor(DTid.xy, float4(1, 1, 1, 1));
        if (GTid.x == 2 && GTid.y == 2) SetColor(DTid.xy, float4(1, 1, 1, 1));
        else SetColor(DTid.xy, float4(0, 0, 1, 1));
        break;
    case SHADING_RATE_2X1:
        SetColor(DTid.xy, float4(0, 0, 1, 1));
        break;
    case SHADING_RATE_2X2:
        SetColor(DTid.xy, float4(0, 1, 0, 1));
        break;
    case SHADING_RATE_2X4:
        if (GTid.x == 1 && GTid.y == 1) SetColor(DTid.xy, float4(1, 1, 1, 1));
        if (GTid.x == 1 && GTid.y == 2) SetColor(DTid.xy, float4(1, 1, 1, 1));
        if (GTid.x == 2 && GTid.y == 1) SetColor(DTid.xy, float4(1, 1, 1, 1));
        if (GTid.x == 2 && GTid.y == 2) SetColor(DTid.xy, float4(1, 1, 1, 1));
        else SetColor(DTid.xy, float4(1, 0, 0, 1));
        break;
    case SHADING_RATE_4X2:
        SetColor(DTid.xy, float4(1, 0, 0, 1));
        break;
    case SHADING_RATE_4X4:
        SetColor(DTid.xy, float4(1, 0, 1, 1));
        break;
    }

    if (GTid.x == 0) SetColor(DTid.xy, float4(0, 0, 0, 1));
    if (GTid.y == 0) SetColor(DTid.xy, float4(0, 0, 0, 1));
}