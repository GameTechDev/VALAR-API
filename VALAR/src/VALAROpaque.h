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
#pragma once

using namespace Microsoft::WRL;

#define VALAR_SAFE_RELEASE(obj) if(obj != nullptr) { obj->Release(); obj.Detach();  }

#define INTEL_TILE_SIZE 8
#define OTHER_TILE_SIZE 16

namespace Intel
{
    struct VALAR_DESCRIPTOR_OPAQUE
    {
        ComPtr<ID3D12Device>        m_device;
        ComPtr<ID3D12RootSignature> m_valarRootSignature;
        ComPtr<ID3D12RootSignature> m_valarDebugRootSignature;
        ComPtr<ID3D12PipelineState> m_valarShaderPermutations[VALAR_SHADER_COUNT];
        VALAR_HARDWARE_FEATURES     m_featureSupport{};
        bool                        m_isInitialized = false;
    };

    struct VALAR_ROOT_CONSTANTS
    {
        UINT                        m_width;
        UINT                        m_height;
        UINT                        m_shadingRateTileSize;
        float                       m_sensitivityThreshold;
        float                       m_environmentLuminance;
        float                       m_quarterRateShadingModifier;
        float                       m_weberFechnerConstant;
        UINT                        m_weberFechnerMode;
        UINT                        m_useMotionVectors;
        UINT                        m_allowQuarterRateShading;
        UINT                        m_upscaledWidth;
        UINT                        m_upscaledHeight;
        UINT                        m_useHighResMotionVectors;          
    };

    struct VALAR_DEBUG_CONSTANTS
    {
        UINT m_nativeWidth;
        UINT m_nativeHeight;

        UINT m_upscaledWidth;
        UINT m_upscaledHeight;

        UINT m_shadingRateTileSize;

        bool m_debugDrawGrid;
    };

    VALAR_RETURN_CODE CheckHardwareSupport(VALAR_DESCRIPTOR& desc, VALAR_HARDWARE_FEATURES& featureSupport);
    VALAR_RETURN_CODE CreateVALARRootSignature(VALAR_DESCRIPTOR& desc);
    VALAR_RETURN_CODE CreateVALARDebugRootSignature(VALAR_DESCRIPTOR& desc);
    VALAR_RETURN_CODE LoadShader(VALAR_DESCRIPTOR& desc, VALAR_SHADER_PERMUTATIONS permutation);
}