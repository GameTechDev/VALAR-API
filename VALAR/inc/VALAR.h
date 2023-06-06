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

#define USE_EMBEDED_SHADERS
#define USE_DYNAMIC_DESCRIPTOR

namespace Intel
{
    typedef enum VALAR_AXIS_SHADING_RATE {
        VALAR_AXIS_SHADING_RATE_1X = 0,
        VALAR_AXIS_SHADING_RATE_2X = 0x1,
        VALAR_AXIS_SHADING_RATE_4X = 0x2
    } VALAR_AXIS_SHADING_RATE;

    typedef enum VALAR_VARIABLE_SHADING_RATE_TIER {
        VALAR_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED = 0,
        VALAR_VARIABLE_SHADING_RATE_TIER_1 = 1,
        VALAR_VARIABLE_SHADING_RATE_TIER_2 = 2
    } VALAR_VARIABLE_SHADING_RATE_TIER;

    typedef enum VALAR_SHADING_RATE {
        VALAR_SHADING_RATE_1X1 = 0,
        VALAR_SHADING_RATE_1X2 = 0x1,
        VALAR_SHADING_RATE_2X1 = 0x4,
        VALAR_SHADING_RATE_2X2 = 0x5,
        VALAR_SHADING_RATE_2X4 = 0x6,
        VALAR_SHADING_RATE_4X2 = 0x9,
        VALAR_SHADING_RATE_4X4 = 0xa
    } VALAR_SHADING_RATE;

    typedef enum VALAR_SHADING_RATE_COMBINER {
        VALAR_SHADING_RATE_COMBINER_PASSTHROUGH = 0,
        VALAR_SHADING_RATE_COMBINER_OVERRIDE = 1,
        VALAR_SHADING_RATE_COMBINER_MIN = 2,
        VALAR_SHADING_RATE_COMBINER_MAX = 3,
        VALAR_SHADING_RATE_COMBINER_SUM = 4
    } VALAR_SHADING_RATE_COMBINER;

    typedef enum VALAR_RETURN_CODE {
        VALAR_RETURN_CODE_SUCCESS,
        VALAR_RETURN_CODE_ROOTSIG_FAIL,
        VALAR_RETURN_CODE_PSO_FAIL,
        VALAR_RETURN_CODE_INVALID_ARGUMENT,
        VALAR_RETURN_CODE_INVALID_DEVICE,
        VALAR_RETURN_CODE_NOT_SUPPORTED,
        VALAR_RETURN_CODE_INITIALIZED,
        VALAR_RETURN_CODE_NOT_INITIALIZED,
        VALAR_RETURN_CODE_MAX
    } VALAR_RETURN_CODE;

    typedef enum VALAR_SHADER_PERMUTATIONS
    {
        VALAR_SHADER_8X8,
        VALAR_SHADER_16X16,
        VALAR_DEBUG_SHADER,
        VALAR_SHADER_COUNT
    } VALAR_SHADER_PERMUTATIONS;

    struct VALAR_DESCRIPTOR_OPAQUE;

    struct VALAR_HARDWARE_FEATURES
    {
        UINT                                m_shadingRateTileSize               = 0;
        bool                                m_additionalShadingRatesSupported   = false;
        bool                                m_sumCombinerSupported              = false;
        bool                                m_meshShaderPerPrimSupported        = false;
        VALAR_VARIABLE_SHADING_RATE_TIER    m_shadingRateTier                   = {};
        bool                                m_vrsTier1Support                   = false;
        bool                                m_vrsTier2Support                   = false;
        bool                                m_isVALARSupported                  = false;
    };

    struct VALAR_DESCRIPTOR
    {
        VALAR_DESCRIPTOR();

        VALAR_SHADING_RATE                  m_baseShadingRate                   = VALAR_SHADING_RATE_1X1;
        float                               m_sensitivityThreshold              = 0.50f;
        float                               m_quarterRateShadingModifier        = 2.13f;
        float                               m_environmentLuminance              = 0.02f;
        bool                                m_allowQuarterRateShading           = true;
        bool                                m_weberFechnerMode                  = false;
        float                               m_weberFechnerConstant              = 1.0f;
        bool                                m_useMotionVectors                  = false;
        bool                                m_useUpscaleMotionVectors           = false;
        bool                                m_debugOverlay                      = false;
        bool                                m_debugGrid                         = false;
        bool                                m_enabled                           = true;
        UINT                                m_bufferWidth                       = 0;
        UINT                                m_bufferHeight                      = 0;
        UINT                                m_upscaleWidth                      = 0;
        UINT                                m_upscaleHeight                     = 0;
        ID3D12Device*                       m_device                            = nullptr;
        ID3D12DescriptorHeap*               m_uavHeap                           = nullptr;
        ID3D12Resource*                     m_valarBuffer                       = nullptr;
        ID3DBlob*                           m_shaderBlobs[VALAR_SHADER_COUNT];
        ID3D12GraphicsCommandList5*         m_commandList                       = nullptr;
        VALAR_DESCRIPTOR_OPAQUE*            m_pOpaque;
        VALAR_HARDWARE_FEATURES             m_hwFeatures;
    };

    const VALAR_RETURN_CODE VALAR_CheckSupport(VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_Initialize(VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_Release(const VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_ComputeMask(const VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_DebugOverlay(const VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_ApplyMask(const VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_ResetMask(const VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_SetScreenSpaceCombiners(const VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_SetHeroAssetCombiners(const VALAR_DESCRIPTOR& desc);
    const VALAR_RETURN_CODE VALAR_SetCustomCombiners(const VALAR_DESCRIPTOR& desc, const VALAR_SHADING_RATE_COMBINER combiner1, const VALAR_SHADING_RATE_COMBINER combiner2);
}