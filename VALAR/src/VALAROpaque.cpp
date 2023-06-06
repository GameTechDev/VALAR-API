// Copyright (C) 2023 Intel Corporation

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

#include <wrl.h>
#include <d3d12.h>
#include <cassert>

#include "VALAR.h"
#include "VALAROpaque.h"

#include "..\..\ThirdParty\d3dx12.h"

#ifdef USE_EMBEDED_SHADERS
    #include "Valar8x8CS.h"
    #include "Valar16x16CS.h"
    #include "ValarDebugCS.h"
#endif

Intel::VALAR_DESCRIPTOR::VALAR_DESCRIPTOR()
{
    static VALAR_DESCRIPTOR_OPAQUE opaque;

    m_pOpaque = &opaque;
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_CheckSupport(VALAR_DESCRIPTOR& desc)
{
    if (desc.m_device == nullptr) {
        return VALAR_RETURN_CODE_INVALID_DEVICE;
    }

    return CheckHardwareSupport(desc, desc.m_hwFeatures);
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_Initialize(Intel::VALAR_DESCRIPTOR & desc)
{
    if (!desc.m_pOpaque->m_isInitialized) {
        if (desc.m_device == nullptr) {
            return VALAR_RETURN_CODE_INVALID_DEVICE;
        }
        
        VALAR_RETURN_CODE retCode = CheckHardwareSupport(desc, desc.m_hwFeatures);
        if (retCode != VALAR_RETURN_CODE_SUCCESS) {
            return VALAR_RETURN_CODE_NOT_SUPPORTED;
        }
        
        retCode = CreateVALARRootSignature(desc);
        if (retCode != VALAR_RETURN_CODE_SUCCESS) {
            return retCode;
        }

        retCode = CreateVALARDebugRootSignature(desc);
        if (retCode != VALAR_RETURN_CODE_SUCCESS) {
            return retCode;
        }

        if (desc.m_hwFeatures.m_shadingRateTileSize == INTEL_TILE_SIZE) {
            retCode = LoadShader(desc, VALAR_SHADER_8X8);
            if (retCode != VALAR_RETURN_CODE_SUCCESS) {
                return retCode;
            }
        } else {
            retCode = LoadShader(desc, VALAR_SHADER_16X16);
            if (retCode != VALAR_RETURN_CODE_SUCCESS) {
                return retCode;
            }
        }

        retCode = LoadShader(desc, VALAR_DEBUG_SHADER);
        if (retCode != VALAR_RETURN_CODE_SUCCESS) {
            return retCode;
        }

        desc.m_pOpaque->m_device = desc.m_device;
        desc.m_pOpaque->m_isInitialized = true;
        desc.m_pOpaque->m_featureSupport = desc.m_hwFeatures;
    } else {
        return VALAR_RETURN_CODE_INITIALIZED;
    }

    return Intel::VALAR_RETURN_CODE_SUCCESS;
}

Intel::VALAR_RETURN_CODE Intel::CheckHardwareSupport(Intel::VALAR_DESCRIPTOR& desc, Intel::VALAR_HARDWARE_FEATURES& featureSupport)
{
     Intel::VALAR_RETURN_CODE retCode = VALAR_RETURN_CODE_NOT_SUPPORTED;

     // Check for VRS hardware support
     D3D12_FEATURE_DATA_D3D12_OPTIONS6 options = {};
     D3D12_FEATURE_DATA_D3D12_OPTIONS10 options2 = {};

     featureSupport.m_shadingRateTier = (VALAR_VARIABLE_SHADING_RATE_TIER)D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
     featureSupport.m_additionalShadingRatesSupported = 0;
     featureSupport.m_shadingRateTileSize = 0;

     if (SUCCEEDED(desc.m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options, sizeof(options)))) {
         featureSupport.m_shadingRateTier = (VALAR_VARIABLE_SHADING_RATE_TIER)options.VariableShadingRateTier;

         if (featureSupport.m_shadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_1) {
             featureSupport.m_additionalShadingRatesSupported = options.AdditionalShadingRatesSupported;
             featureSupport.m_vrsTier1Support = true;
         }

         if (featureSupport.m_shadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_2) {
             featureSupport.m_shadingRateTileSize = options.ShadingRateImageTileSize;
             featureSupport.m_vrsTier2Support = true;
             featureSupport.m_isVALARSupported = true;
         }

         retCode = VALAR_RETURN_CODE_SUCCESS;
     }

     if (SUCCEEDED(desc.m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS10, &options2, sizeof(options2)))) {
         featureSupport.m_sumCombinerSupported = options2.VariableRateShadingSumCombinerSupported;
         featureSupport.m_meshShaderPerPrimSupported = options2.MeshShaderPerPrimitiveShadingRateSupported;
     }

     return retCode;
 }

 Intel::VALAR_RETURN_CODE Intel::CreateVALARRootSignature(Intel::VALAR_DESCRIPTOR& desc)
 {
     ComPtr<ID3DBlob> signature, errors;

     CD3DX12_DESCRIPTOR_RANGE1 descRange[2] = {};
     CD3DX12_ROOT_PARAMETER1 rootParams[2] = {};
     D3D12_STATIC_SAMPLER_DESC sampler = {};
     D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
     CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;

     featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

     if (FAILED(desc.m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
         featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
     }

     descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0);
     descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 13, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

     rootParams[0].InitAsConstants(13, 0);
     rootParams[1].InitAsDescriptorTable(1, &descRange[0]);
     rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

     if (FAILED(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &errors))) {
         return VALAR_RETURN_CODE_ROOTSIG_FAIL;
     }

     auto sig = desc.m_pOpaque->m_valarRootSignature.Get();

     if (FAILED(desc.m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&sig)))) {
         return VALAR_RETURN_CODE_ROOTSIG_FAIL;
     }
     desc.m_pOpaque->m_valarRootSignature = sig;

     return VALAR_RETURN_CODE_SUCCESS;
 }

 Intel::VALAR_RETURN_CODE Intel::CreateVALARDebugRootSignature(Intel::VALAR_DESCRIPTOR& desc)
 {
     ComPtr<ID3DBlob> signature, errors;

     CD3DX12_DESCRIPTOR_RANGE1 descRange[2] = {};
     CD3DX12_ROOT_PARAMETER1 rootParams[2] = {};
     D3D12_STATIC_SAMPLER_DESC sampler = {};
     D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
     CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;

     featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

     if (FAILED(desc.m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
         featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
     }

     descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);
     descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 6, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

     rootParams[0].InitAsConstants(6, 0);
     rootParams[1].InitAsDescriptorTable(1, &descRange[0]);
     rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

     if (FAILED(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &errors))) {
         return VALAR_RETURN_CODE_ROOTSIG_FAIL;
     }

     auto sig = desc.m_pOpaque->m_valarDebugRootSignature.Get();

     if (FAILED(desc.m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&sig)))) {
         return VALAR_RETURN_CODE_ROOTSIG_FAIL;
     }
     desc.m_pOpaque->m_valarDebugRootSignature = sig;

     return VALAR_RETURN_CODE_SUCCESS;
 }

Intel::VALAR_RETURN_CODE Intel::LoadShader(Intel::VALAR_DESCRIPTOR& desc, VALAR_SHADER_PERMUTATIONS permutation)
{
    UINT8* pComputeShaderData = nullptr;
    UINT computeShaderDataLength = 0;

#ifdef USE_EMBEDED_SHADERS
    switch (permutation)
    {
    case VALAR_SHADER_8X8:
        pComputeShaderData = (UINT8*)g_valar8x8ByteCode;
        computeShaderDataLength = sizeof(g_valar8x8ByteCode) / sizeof(const unsigned char);
        break;
    case VALAR_SHADER_16X16:
        pComputeShaderData = (UINT8*)g_valar16x16ByteCode;
        computeShaderDataLength = sizeof(g_valar16x16ByteCode) / sizeof(const unsigned char);
        break;
    case VALAR_DEBUG_SHADER:
        pComputeShaderData = (UINT8*)g_valar8x8DebugByteCode;
        computeShaderDataLength = sizeof(g_valar8x8DebugByteCode) / sizeof(const unsigned char);
        break;
    }
#else
    if (desc.m_shaderBlobs[permutation] == nullptr)
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;

    pComputeShaderData = (UINT8*)desc.m_shaderBlobs[permutation]->GetBufferPointer();
    computeShaderDataLength = (UINT)desc.m_shaderBlobs[permutation]->GetBufferSize();
#endif

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};

    if (permutation == VALAR_DEBUG_SHADER) {
        psoDesc.pRootSignature = desc.m_pOpaque->m_valarDebugRootSignature.Get();
    } else {
        psoDesc.pRootSignature = desc.m_pOpaque->m_valarRootSignature.Get();
    }

    psoDesc.CS = CD3DX12_SHADER_BYTECODE(pComputeShaderData, computeShaderDataLength);
   
    if (FAILED(desc.m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&desc.m_pOpaque->m_valarShaderPermutations[permutation])))) {
        return VALAR_RETURN_CODE_PSO_FAIL;
    }

    return VALAR_RETURN_CODE_SUCCESS;
}


const Intel::VALAR_RETURN_CODE Intel::VALAR_Release(const Intel::VALAR_DESCRIPTOR& desc)
{
    VALAR_RETURN_CODE retCode = VALAR_RETURN_CODE_NOT_INITIALIZED;

    if (desc.m_pOpaque->m_isInitialized)
    {
        VALAR_SAFE_RELEASE(desc.m_pOpaque->m_device);
        VALAR_SAFE_RELEASE(desc.m_pOpaque->m_valarRootSignature);
        VALAR_SAFE_RELEASE(desc.m_pOpaque->m_valarDebugRootSignature);
        VALAR_SAFE_RELEASE(desc.m_pOpaque->m_valarShaderPermutations[VALAR_SHADER_8X8]);
        VALAR_SAFE_RELEASE(desc.m_pOpaque->m_valarShaderPermutations[VALAR_SHADER_16X16]);
        VALAR_SAFE_RELEASE(desc.m_pOpaque->m_valarShaderPermutations[VALAR_DEBUG_SHADER]);

        desc.m_pOpaque->m_isInitialized = false;
        retCode = VALAR_RETURN_CODE_SUCCESS;
    }

    return retCode;
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_DebugOverlay(const VALAR_DESCRIPTOR& desc)
{
    if (!desc.m_hwFeatures.m_vrsTier2Support) {
        return VALAR_RETURN_CODE_NOT_SUPPORTED;
    }

    if (desc.m_commandList == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }

    if (desc.m_pOpaque->m_device == nullptr) {
        return VALAR_RETURN_CODE_INVALID_DEVICE;
    }

    if (!desc.m_pOpaque->m_isInitialized) {
        return VALAR_RETURN_CODE_NOT_INITIALIZED;
    }

    if (desc.m_enabled && desc.m_debugOverlay)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(desc.m_valarBuffer,
            D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        desc.m_commandList->ResourceBarrier(1, &barrier);

        ID3D12DescriptorHeap* ppHeapsCompute[] = { desc.m_uavHeap };
        desc.m_commandList->SetDescriptorHeaps(_countof(ppHeapsCompute), ppHeapsCompute);
        desc.m_commandList->SetComputeRootSignature(desc.m_pOpaque->m_valarDebugRootSignature.Get());

        VALAR_DEBUG_CONSTANTS constants =
        {
            desc.m_bufferWidth,
            desc.m_bufferHeight,

            desc.m_upscaleWidth,
            desc.m_upscaleHeight,

            desc.m_pOpaque->m_featureSupport.m_shadingRateTileSize,

            desc.m_debugGrid,
        };

        desc.m_commandList->SetComputeRoot32BitConstants(0, 6, &constants, 0);
        desc.m_commandList->SetComputeRootDescriptorTable(1, desc.m_uavHeap->GetGPUDescriptorHandleForHeapStart());
        desc.m_commandList->SetPipelineState(desc.m_pOpaque->m_valarShaderPermutations[VALAR_DEBUG_SHADER].Get());

        desc.m_commandList->Dispatch(
            (UINT)desc.m_upscaleWidth, (UINT)desc.m_upscaleHeight, 1);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(desc.m_valarBuffer,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
        desc.m_commandList->ResourceBarrier(1, &barrier);
    }

    return VALAR_RETURN_CODE_SUCCESS;
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_ComputeMask(const Intel::VALAR_DESCRIPTOR& desc)
{
    if (!desc.m_hwFeatures.m_vrsTier2Support) {
        return VALAR_RETURN_CODE_NOT_SUPPORTED;
    }

    if (desc.m_commandList == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }
        
    if (desc.m_valarBuffer == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }
    
    if (desc.m_uavHeap == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }

    if (desc.m_pOpaque->m_device == nullptr) {
        return VALAR_RETURN_CODE_INVALID_DEVICE;
    }

    if (!desc.m_pOpaque->m_isInitialized) {
        return VALAR_RETURN_CODE_NOT_INITIALIZED;
    }

    if (desc.m_enabled) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(desc.m_valarBuffer,
            D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        desc.m_commandList->ResourceBarrier(1, &barrier);

        ID3D12DescriptorHeap* ppHeapsCompute[] = { desc.m_uavHeap };
        desc.m_commandList->SetDescriptorHeaps(_countof(ppHeapsCompute), ppHeapsCompute);
        desc.m_commandList->SetComputeRootSignature(desc.m_pOpaque->m_valarRootSignature.Get());

        VALAR_ROOT_CONSTANTS constants =
        {
            desc.m_bufferWidth,
            desc.m_bufferHeight,
            (UINT)desc.m_pOpaque->m_featureSupport.m_shadingRateTileSize,
            desc.m_sensitivityThreshold,
            desc.m_environmentLuminance,
            desc.m_quarterRateShadingModifier,
            desc.m_weberFechnerConstant,
            desc.m_weberFechnerMode,
            desc.m_useMotionVectors,
            desc.m_allowQuarterRateShading,
            desc.m_upscaleWidth,
            desc.m_upscaleHeight,
            desc.m_useUpscaleMotionVectors
        };

        desc.m_commandList->SetComputeRoot32BitConstants(0, 13, &constants, 0);
        desc.m_commandList->SetComputeRootDescriptorTable(1, desc.m_uavHeap->GetGPUDescriptorHandleForHeapStart());

        if (desc.m_pOpaque->m_featureSupport.m_shadingRateTileSize == INTEL_TILE_SIZE) {
            desc.m_commandList->SetPipelineState(desc.m_pOpaque->m_valarShaderPermutations[VALAR_SHADER_8X8].Get());
        } else if (desc.m_pOpaque->m_featureSupport.m_shadingRateTileSize == OTHER_TILE_SIZE) {
            desc.m_commandList->SetPipelineState(desc.m_pOpaque->m_valarShaderPermutations[VALAR_SHADER_16X16].Get());
        } else {
            assert(false);
        }

        desc.m_commandList->Dispatch(
            (UINT)ceilf((float)desc.m_bufferWidth / (float)desc.m_pOpaque->m_featureSupport.m_shadingRateTileSize),
            (UINT)ceilf((float)desc.m_bufferHeight / (float)desc.m_pOpaque->m_featureSupport.m_shadingRateTileSize), 1);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(desc.m_valarBuffer,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
        desc.m_commandList->ResourceBarrier(1, &barrier);
    }

    return VALAR_RETURN_CODE_SUCCESS;
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_ApplyMask(const Intel::VALAR_DESCRIPTOR& desc)
{
    if (!desc.m_enabled) {
        return VALAR_RETURN_CODE_SUCCESS;
    }

    if (!desc.m_hwFeatures.m_vrsTier2Support) {
        return VALAR_RETURN_CODE_NOT_SUPPORTED;
    }

    if (desc.m_commandList == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }    
    
    if (desc.m_valarBuffer == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }

    if (!desc.m_pOpaque->m_isInitialized) {
        return VALAR_RETURN_CODE_NOT_INITIALIZED;
    }

    desc.m_commandList->RSSetShadingRateImage(desc.m_valarBuffer);

    return VALAR_RETURN_CODE_SUCCESS;
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_ResetMask(const Intel::VALAR_DESCRIPTOR& desc)
{
    if (!desc.m_hwFeatures.m_vrsTier2Support) {
        return VALAR_RETURN_CODE_NOT_SUPPORTED;
    }

    if (desc.m_commandList == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }

    if (!desc.m_pOpaque->m_isInitialized) {
        return VALAR_RETURN_CODE_NOT_INITIALIZED;
    }

    desc.m_commandList->RSSetShadingRateImage(nullptr);

    return VALAR_RETURN_CODE_SUCCESS;
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_SetScreenSpaceCombiners(const Intel::VALAR_DESCRIPTOR& desc)
{
    if (!desc.m_hwFeatures.m_vrsTier1Support) {
        return VALAR_RETURN_CODE_NOT_SUPPORTED;
    }

    if (desc.m_commandList == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }

    if (!desc.m_pOpaque->m_isInitialized) {
        return VALAR_RETURN_CODE_NOT_INITIALIZED;
    }

    D3D12_SHADING_RATE_COMBINER combiners[2] = { D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_OVERRIDE };

    desc.m_commandList->RSSetShadingRate((D3D12_SHADING_RATE)desc.m_baseShadingRate, combiners);
    
    return VALAR_RETURN_CODE_SUCCESS;
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_SetHeroAssetCombiners(const Intel::VALAR_DESCRIPTOR& desc)
{
    if (!desc.m_hwFeatures.m_vrsTier1Support) {
        return VALAR_RETURN_CODE_NOT_SUPPORTED;
    }

    if (desc.m_commandList == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }

    if (!desc.m_pOpaque->m_isInitialized) {
        return VALAR_RETURN_CODE_NOT_INITIALIZED;
    }

    D3D12_SHADING_RATE_COMBINER combiners[2] = { D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH };

    desc.m_commandList->RSSetShadingRate((D3D12_SHADING_RATE)desc.m_baseShadingRate, combiners);

    return VALAR_RETURN_CODE_SUCCESS;
}

const Intel::VALAR_RETURN_CODE Intel::VALAR_SetCustomCombiners(const Intel::VALAR_DESCRIPTOR& desc, const VALAR_SHADING_RATE_COMBINER combiner1, const VALAR_SHADING_RATE_COMBINER combiner2)
{
    if (!desc.m_hwFeatures.m_vrsTier1Support) {
        return VALAR_RETURN_CODE_NOT_SUPPORTED;
    }

    if (desc.m_commandList == nullptr) {
        return VALAR_RETURN_CODE_INVALID_ARGUMENT;
    }

    if (!desc.m_pOpaque->m_isInitialized) {
        return VALAR_RETURN_CODE_NOT_INITIALIZED;
    }

    D3D12_SHADING_RATE_COMBINER combiners[2] = { 
        (D3D12_SHADING_RATE_COMBINER)combiner1, 
        (D3D12_SHADING_RATE_COMBINER)combiner2 };

    desc.m_commandList->RSSetShadingRate((D3D12_SHADING_RATE)desc.m_baseShadingRate, combiners);

    return VALAR_RETURN_CODE_SUCCESS;
}
