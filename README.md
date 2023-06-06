
# Intel's Velocity & Luminance Adaptive Rasterization (VALAR) API

## Introduction

With the introduction of Variable Rate Shading Tier 2 on Intel Arc A-Series discrete GPUs it is now possible to render images with fewer pixel shader invocations at runtime. Intel now offers a VRS Tier 2 API based on our Velocity & Luminance Adaptive Rasterization algorithm. 

https://www.intel.com/content/www/us/en/developer/articles/technical/velocity-luminance-adaptive-rasterization-vrs-tier.html 

The VALAR API can query for VRS Tier 2 Hardware Support, Render VRS Tier 2 Masks with Intel's VALAR algorithm, as well as control VRS Combiners to include or exclude draw calls from the VRS Tier 2 mask. We chose an opaque descriptor pattern to implement the API which should allow multi-GPU support for VRS Tier 2.

## Screenshots

### 2x2 Only Mode
![Alt text](/VALAR/img/VALAR_Screenshot_1080p_2x2Only.png?raw=true "2x2 Only Mode")

### Allow Quarter Rate Shading
![Alt text](/VALAR/img/VALAR_Screenshot_1080p_Allow4x4.png?raw=true "Allow Quarter Rate Shading")

## Limitations

It is important to understand that there are limitations to the VALAR API. 

- Requires VRS Tier 2 Capable GPU
- DirectX 12 Only

## Initializing the API

### The ```VALAR_DESCRIPTOR``` Struct

The VALAR API relies on an opaque descriptor pattern. As such each function in the API requires a descriptor parameter. Upon initialization the ```VALAR_DESCRITPOR``` struct will store a ```ID3D12Device``` in an internal "opaque" descriptor. Once the descriptor is initialized, it needs to be kept in memory and re-used for each subsequent call to the VALAR API. 

```c++
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
```
The ```VALAR_DESCRIPTOR``` contains parameters to control the settings for the VALAR algorithm. To enable or disable the VALAR API use the Enabled (```m_enabled```) parameter, additionally the debug overlay can be enabled or disabled by settings the Debug Overlay (```m_debugOverlay```) parameter.

The sensitivity of the VALAR algorithm is controlled through several floating point parameters; these include the Sensitivity Threshold (```m_sensitivityThreshold```), the Quarter Rate Shading Modifier (```m_quarterRateShadingModifier```), Additive Environment Luminance (```m_environmentLuminance```).. 

There are several parameters that also control the behavior of the VALAR API and the VALAR Algorithm. The Base Shading Rate (```m_baseShadingRate```) parameter works when setting VRS Combiners. Allow Quarter Rate Shading (```m_allowQuarterRateShading```) with toggle the use of the 2x4, 4x2, and 4x4 shading rates in the mask. The Weber-Fechner Mode paramter (```m_weberFechnerMode```) allows you to control the precision of the VALAR algorithm using the Weber-Fechner Constant (```m_weberFechnerConstant```). 

The use of motion vectors can be enabled or disabled using the Use Motion Vectors (```m_useMotionVectors```) and Use Upscale Motion Vectors (```m_useUpscaleMotionVectors```) parameters; both toggle the behavior of the velocity portions of the VALAR algorithm. When using Upscaled Motion Vectors you must also supply an upscaled buffer width (```m_upscaleWidth```) and height (```m_upscaleHeight```). 

Several ID3D12 objects are used to parameterize the D3D12 interface for the VALAR API, which include the device (```m_device```), a UAV heap (```m_uavHeap```), the VRS Buffer (```m_valarBuffer```), the color buffer (```m_colorBuffer```), the velocity buffer (```m_velocityBuffer```), optional shader blobs (```m_shaderBlobs```) for custom shader loading, and a command list (```m_commandList```). These parameters will be covered in the following sections. 

After initializing, a reference to the opaque descriptor (```m_pOpaque```) is kept and a hardware feature (```m_hwFeatures```) struct is returned which stores device capabilities of the device.

### Initializing ```VALAR_DESCRIPTOR``` Struct

To Initialize the VALAR API create a ```VALAR_DESCRIPTOR``` type and set the ```m_device``` parameter to the chosen device. Once the descriptor is setup, pass it to the ```Intel::VALAR_Initialize``` function to initialize the opaque descriptor and determine hardware feature support for VRS.

```c++
// Provide the D3D Device that will use VALAR
m_valarDescriptor.m_device = m_d3dDevice.Get();

// Check VRS Hardware Features & Initialize the Opaque Descriptor
Intel::VALAR_RETURN_CODE returnCode = Intel::VALAR_Initialize(m_valarDescriptor);
```

The ```Intel::VALAR_Initialize``` function will return ```VALAR_RETURN_CODE_SUCCESS``` if VRS Tier 2 is supported and initialization was successful. 

If initialization fails the following return codes may be generated. 

* ```VALAR_RETURN_CODE_INVALID_DEVICE``` indicates that the device passed in the descriptor is ```nullptr```
* ```VALAR_RETURN_CODE_NOT_SUPPORTED``` indicates that VRS Tier 2 is not support on the device.
* ```VALAR_RETURN_CODE_INITIALIZED``` indicates the descriptor passed into the function has already been initialized.
* ```VALAR_RETURN_CODE_INVALID_ARGUMENT``` indicates that one or more of the optional Shader Blob parameters were ```nullptr```
* ```VALAR_RETURN_CODE_ROOTSIG_FAIL``` indicates that there was an error in root signature creation.
* ```VALAR_RETURN_CODE_PSO_FAIL``` indicates that either the optional shader blobs or the embeded shaders for VALAR failed during PSO creation.

Additionally, you can initialize the VALAR API for multiple devices, but it requires multiple VALAR descriptors to be initialized. 

```c++
// Provide the first D3D Device that will use VALAR
m_valarDescriptor1.m_device = m_d3dDevice1.Get();

// Check VRS Hardware Features & Initialize the Opaque Descriptor for the First Device
Intel::VALAR_RETURN_CODE returnCode = Intel::VALAR_Initialize(m_valarDescriptor1);

// Provide the second D3D Device that will use VALAR
m_valarDescriptor2.m_device = m_d3dDevice2.Get();

// Check VRS Hardware Features & Initialize the Opaque Descriptor for the Second Device
Intel::VALAR_RETURN_CODE returnCode = Intel::VALAR_Initialize(m_valarDescriptor2);

// Initialize Additional Devices As needed
// ...
```

### ```VALAR_DESCRIPTOR``` PSO Initialization

There are four shaders that are used to support VALAR at runtime, there are two VALAR compute shaders for the common shading rate tile sizes of 8x8 and 16x16 along with two VALAR debug compute shaders that also come in 8x8 and 16x16 versions. 

* ```Valar8x8CS.hlsl``` VALAR Compute Shader for 8x8 Shading Rate Tile Size (Supported by Intel) 
* ```Valar16x16CS.hlsl``` VALAR Compute Shader for 16x16 Shading Rate Tile Size (Other Vendors)
* ```ValarDebugCS.hlsl``` VALAR Debug Overlay Shader for 8x8 & 16x16 Shading Rate Tile Size

By default these shaders are embedded into the ```.lib``` file generated at compile time. The API uses the ```#define EMBED_VALAR_SHADERS``` to control the inclusion of the embedded shaders. However, if ```EMBED_VALAR_SHADERS``` is not defined shader blobs must be provided at initialize time. Failure to supply blobs in the VALAR descriptor will result in a ```VALAR_RETURN_CODE_PSO_FAIL``` return code. For example, the following code initializes the VALAR API using byte code arrays as ```ID3DBlobs```. It is up to the application programmer to determine how to load the byte code arrays at runtime.

```c++
// Provide the D3D Device that will use VALAR
m_valarDescriptor.m_device = m_d3dDevice.Get();

// Provide the VALAR 8x8 Shader Blob Byte Code.
size[VALAR_SHADER_8X8] = sizeof(g_valar8x8ByteCode) / sizeof(const unsigned char);
D3DCreateBlob(size[VALAR_SHADER_8X8], &m_valarDescriptor.m_shaderBlobs[VALAR_SHADER_8X8]);
memcpy(m_valarDescriptor.m_shaderBlobs[VALAR_SHADER_8X8]->GetBufferPointer(), g_valar8x8ByteCode, size[VALAR_SHADER_8X8]);

// Provide the VALAR 16x16 Shader Blob Byte Code.
size[VALAR_SHADER_16X16] = sizeof(g_valar16x16ByteCode) / sizeof(const unsigned char);
D3DCreateBlob(size[VALAR_SHADER_16X16], &m_valarDescriptor.m_shaderBlobs[VALAR_SHADER_16X16]);
memcpy(m_valarDescriptor.m_shaderBlobs[VALAR_SHADER_16X16]->GetBufferPointer(), g_valar16x16ByteCode, size[VALAR_SHADER_16X16]);

// Provide the VALAR Debug Shader Blob Byte Code.
size[VALAR_DEBUG_SHADER] = sizeof(g_valarDebugByteCode) / sizeof(const unsigned char);
D3DCreateBlob(size[VALAR_DEBUG_SHADER], &m_valarDescriptor.m_shaderBlobs[VALAR_DEBUG_SHADER]);
memcpy(m_valarDescriptor.m_shaderBlobs[VALAR_DEBUG_SHADER]->GetBufferPointer(), g_valarDebugByteCode, size[VALAR_DEBUG_SHADER]);

// Check VRS Hardware Features & Initialize the Opaque Descriptor with Custom Shaders
Intel::VALAR_RETURN_CODE returnCode = Intel::VALAR_Initialize(m_valarDescriptor);
```

Once VALAR initialization is successful be sure to keep a reference to the VALAR descriptor object to use later when generating and applying masks. 

### VRS Hardware Feature Support

After the ```VALAR_DESCRIPTOR``` has been successfully initialized, the descriptor will contain a ```VALAR_HARDWARE_FEATURES``` struct in the ```m_hwFeatures``` field, that can be used to examine hardware support for variable rate shading. 

```c++
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
```
The shading rate tile size is returned in ```m_shadingRateTileSize``` and should be used to initialize VRS Buffers used to generate the VALAR mask. The supported shading rate tier is returned in m_shadingRateTier, however, there are also boolean fields to explicitly determine if VRS Tier 1 (```m_vrsTier1Support```) or VRS Tier 2 (```m_vrsTier2Support```)is supported. ```m_isVALARSupported``` will indicate if there is sufficient hardware support for the VALAR algorithm to work correctly on the device. Additional shading rate support is indicated by ```m_additionalShadingRatesSupported``` and determines if 2x4, 4x2, and 4x4 shading rates are supported. Additionally, a few other VRS related hardware features are detected for convenience such as VRS Sum Combiner Support (```m_sumCombinerSupported```) and VRS Mesh Shader Per-Primitive Support (```m_meshShaderPerPrimSupported```). At runtime values can be reported or logged similar to the following example

```c++
Logger::WriteLine("VALAR Init Succeeded");
Logger::WriteLine("Tier1 Supported: {0}", m_valarDescriptor.m_hwFeatures.m_vrsTier1Support ? "True" : "False");
Logger::WriteLine("T14x4 Supported: {0}", m_valarDescriptor.m_hwFeatures.m_additionalShadingRatesSupported ? "True" : "False");
Logger::WriteLine("Tier2 Supported: {0}", m_valarDescriptor.m_hwFeatures.m_vrsTier2Support ? "True" : "False");
Logger::WriteLine("Tier2 Tile Size: {0}", m_valarDescriptor.m_hwFeatures.m_shadingRateTileSize ? "True" : "False");
Logger::WriteLine("VALAR Supported: {0}", m_valarDescriptor.m_hwFeatures.m_isVALARSupported ? "True" : "False");
Logger::WriteLine("T2Sum Combiners: {0}", m_valarDescriptor.m_hwFeatures.m_sumCombinerSupported ? "True" : "False");
Logger::WriteLine("MeshShader Prim: {0}", m_valarDescriptor.m_hwFeatures.m_meshShaderPerPrimSupported ? "True" : "False");
```

## Generate a VALAR Mask

Once a ```VALAR_DESCRIPTOR``` has been initialized it is possible to generate a VALAR mask. In addition to providing an initialized descriptor the application is also responsible for supplying a Graphics Command List (```m_commandList```), a 4 slot UAV Heap (```m_uavHeap```), and a VRS Buffer (```m_valarBuffer```). 

### VALAR Descriptor Heap Setup

The ```m_valarHeap``` paramter must contain at least two UAVs; Slot 0 is reserved for the VALAR buffer, Slot 1 is reserved for the native resolution color buffer, and optionally Slot 3 is reserved for native resolution motion vectors, while slot 4 can optionally be used with XeSS to provide upscaled motion vectors.

```c++
 auto uavDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
uavDesc.Texture2DArray.PlaneSlice = 0;
uavDesc.Texture2DArray.FirstArraySlice = 0;
uavDesc.Texture2DArray.MipSlice = 0;
uavDesc.Texture2DArray.ArraySize = 1;

// Use UAV Slot 0 to pass in VALAR buffer UAV
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_valarDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, uavDescriptorSize);

    uavDesc.Format = DXGI_FORMAT_R8_UINT;

    g_Device->CreateUnorderedAccessView(g_VRSTier2Buffer.GetResource(), nullptr, &uavDesc, uavHandle);
}

// Use UAV Slot 1 to pass in Color buffer UAV
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_valarDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, uavDescriptorSize);

    uavDesc.Format = Target.GetFormat();

    g_Device->CreateUnorderedAccessView( Target.GetResource(), nullptr, &uavDesc, uavHandle);
}

// Use UAV Slot 2 to pass in Velocity buffer UAV
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_valarDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 2, uavDescriptorSize);

    uavDesc.Format = DXGI_FORMAT_R32_UINT;

    g_Device->CreateUnorderedAccessView(g_VelocityBuffer.GetResource(), nullptr, &uavDesc, uavHandle);
}

// Use UAV Slot 3 to pass in Upscaled Velocity buffer UAV
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_valarDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 3, uavDescriptorSize);

    uavDesc.Format = DXGI_FORMAT_R16G16_FLOAT;

    g_Device->CreateUnorderedAccessView(g_UpscaledVelocityBuffer.GetResource(), nullptr, &uavDesc, uavHandle);
}

```

### Setting VALAR Parameters

Once the SRV/UAV heap and buffers are initialized update the descriptor with the descriptor heap (```m_uavHeap```), color buffer (```m_colorBuffer```), vrs buffer (```m_valarBuffer```), and velocity buffers (```m_velocityBuffer```). The width and height of the color buffer should also be provided in the ```m_bufferWidth``` and ```m_bufferHeight``` descriptor parameters.

```c++
Intel::VALAR_DESCRIPTOR& valarDesc = m_valarDescriptor;
valarDesc.m_valarBuffer = m_valarBuffer.Get();
valarDesc.m_uavHeap = m_valarDescriptorHeap.Get();
valarDesc.m_commandList = commandList.Get();
valarDesc.m_bufferHeight = m_height;
valarDesc.m_bufferWidth = m_width;

// ...
```

After rendering to your velocity and color buffers the application should call ```Intel::VALAR_ComputeMask``` with a command list and parameters to control the VALAR algorithm.

```c++
// ...

valarDesc.m_enable = true;
valarDesc.m_commandList = commandList.Get();
valarDesc.m_sensitivityThreshold = 0.5f;
valarDesc.m_quarterRateShadingModifier = 2.13f
valarDesc.m_environmentLuminance = 0.0f;
valarDesc.m_colorBuffer = m_colorBuffer.Get();

// Optional Paramters
// valarDesc.m_useWeberFechner = false;
// valarDesc.m_weberFechnerConstant = 1.0;
// valarDesc.m_allowQuarterRateShading = true;

// ...
// Render Scene
// ...

Intel::VALAR_RETURN_CODE retCode = Intel::VALAR_ComputeMask(valarDesc);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);

// ...
// Post-Process / GUI
// ...
```

### Using Velocity

VALAR supports both native-resolution and XeSS upscaled resolution velocity buffers. As mentioned in the previous section native-resolution velocity is passed in to the compute shader in ```m_uavHeap``` UAV slot 2, while the upscaled motion vectors are passed in UAV slot 3. To enable native resolution motion vectors set ```m_useMotionVectors = true``` and ```m_useUpscaledMotionVectors = false```. To enable XeSS Upscaled resolution motion vectors set ```m_useMotionVectors = true``` and ```m_useUpscaledMotionVectors = true``` and specify upscaled width and height in ```m_upscaledWidth``` and ```m_upscaledHeight```.

```c++

// Enable Native Resolution Motion Vector Support
m_valarDescriptor.m_useMotionVectors = true;
m_valarDescriptor.m_useUpscaleMotionVectors = false;

// ...

// Enable Upscaled Resolution Motion Vector Support
m_valarDescriptor.m_useMotionVectors = true;
m_valarDescriptor.m_useUpscaleMotionVectors = true;
m_valarDescriptor.m_upscaledWidth = m_upscaledWidth;
m_valarDescriptor.m_upscaledHeight = m_upscaledHeight;

Intel::VALAR_RETURN_CODE retCode = Intel::VALAR_ComputeMask(valarDesc);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);

```

```Intel::VALAR_ComputeMask``` will return an return code of ```VALAR_RETURN_CODE_SUCCESS``` if the mask is successfully generated. Otherwise the following VALAR error codes will be returned.

* ```VALAR_RETURN_CODE_NOT_INITIALIZED``` indicates that ```Intel::VALAR_Initialize``` function failed or was never called.
* ```VALAR_RETURN_CODE_INVALID_DEVICE``` indicates that the opaque descriptors internal device is invalid.
* ```VALAR_RETURN_CODE_NOT_SUPPORTED``` indicates that the device does not support VRS Tier 2
* ```VALAR_RETURN_CODE_INVALID_ARGUMENT``` indicates that the Command List, UAV Heap, VRS buffer, or Color buffer is invalid.

Once the ```Intel::VALAR_ComputeMask``` function returns successfully you can apply the mask to any valid graphics command list. 

## Appling a VALAR Mask

After a mask has been generated it needs to be applied to the next frame. Masks can be applied using the ```Intel::VALAR_ApplyMask``` function. Internally ```Intel::VALAR_ApplyMask``` calls ```ID3D12GraphicsCommandList5::RSSetShadingRateImage```. To apply a mask, a valid ```VALAR_DESCRIPTOR``` must be passed with a valid ```ID3D12GraphicsCommandList5``` assigned to ```m_commandList``` parameter along with a valid ```ID3D12Resource``` passed in the ```m_valarBuffer``` parameter.

```c++
valarDesc.m_enable = true;
valarDesc.m_commandList = commandList.Get();
valarDesc.m_valarBuffer = m_valarBuffer.Get();

// ...
// Start Frame
// ...

// ...
// Set VRS Combiners
// ...

Intel::VALAR_RETURN_CODE retCode = Intel::VALAR_ApplyMask(valarDesc);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);

// ...
// Render Frame (With Mask)
// ...

// ...
// Compute Mask For Next Frame
// ... 
```

```Intel::VALAR_ApplyMask``` will return ```VALAR_RETURN_CODE_SUCCESS``` when the mask is successfully applied or the ```m_enable``` flag is set to false. Otherwise, ```Intel::VALAR_ApplyMask``` will return the following error codes.

* ```VALAR_RETURN_CODE_NOT_INITIALIZED``` indicates that the ```VALAR_DESCRIPTOR``` was not properly initialized.
* ```VALAR_RETURN_CODE_NOT_SUPPORTED``` indicates that the device used to initialize VALAR does not support VRS Tier 2
* ```VALAR_RETURN_CODE_INVALID_ARGUMENT``` indicates that either ```m_commandList``` or ```m_valarBuffer``` is ```nullptr```

```Intel::VALAR_ApplyMask``` can also be used to provide a custom mask generated by the application. The mask does not need to be generated by ```Intel::VALAR_ComputeMask``` it just needs to be a valid ```ID3D12Resource``` with a format of ```DXGI_FORMAT_R8_UINT```. Applying the mask can also be done on any valid ```ID3D12GraphicsCommandList5``` and each command list can apply a different mask if needed.

## Resetting the VALAR Mask

In some cases after a mask has been applied using ```Intel::VALAR_ApplyMask``` it may be necessary to disable the mask before a specific render pass is rendered, such as post-processing or a GUI. In this case the application can call ```Intel::VALAR_ResetMask```. The ```Intel::VALAR_ResetMask``` function requires a valid descriptor with ```m_commandList``` to a valid ```ID3D12GraphicsCommandList5```.

```c++
valarDesc.m_enable = true;
valarDesc.m_commandList = commandList.Get();

// ...
// Apply VALAR Mask
// ...

// ...
// Render Frame
// ...

Intel::VALAR_RETURN_CODE retCode = Intel::VALAR_ResetMask(valarDesc);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);

// ...
// Render GUI
// ...
```

```Intel::VALAR_ResetMask``` will return ```VALAR_RETURN_CODE_SUCCESSS``` when called succesfully, otherwise the following error codes will be returned.

* ```VALAR_RETURN_CODE_NOT_INITIALIZED``` indicates the ```VALAR_DESCRIPTOR``` was not properly initialized.
* ```VALAR_RETURN_CODE_NOT_SUPPORTED``` indicates that the device used to initialize the descriptor does not support VRS Tier 2
* ```VALAR_RETURN_CODE_INVALID_ARGUMENT``` indicates that ```m_commandList``` is ```nullptr```.

## Using VALAR Combiners

Before applying a VRS Tier 2 mask it is necessary to properly configure the VRS combiners to allow screen-space shading rates or per-draw shading rates to take precedent. The VALAR API provides a few helper functions to set a "Hero" Passthrough + Passthrough combiner, a screen-space Passthrough + Override combiner, or a custom combiner. Each function requires a valid ```ID3D12GraphicsCommandList5``` pointer to be passed in the ```m_commandList``` field of the VALAR descriptor. Optionally, these functions can also be used to provide a base VRS Tier 1 shading rate to render draw calls with by setting ```m_baseShadingRate``` in the VALAR descriptor struct.

```Intel::VALAR_SetScreenSpaceCombiners``` is a Passthrough + Override combiner which allows the screen-space VRS Tier 2 shading rates to override the per-draw shading rates provided by ```RSSetShadingRate```. Draw calls drawn after applying the Screen-Space combiners will be drawn using the VRS Tier 2 shading rate.

```c++
valarDesc.m_baseShadingRate = VALAR_SHADING_RATE_1X1;
valarDesc.m_commandList = commandList;

retCode = Intel::VALAR_SetScreenSpaceCombiners(valarDesc);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);

// ...
// Apply VALAR Mask
// ...

// ...
// Render Frame
// ...
```

```Intel::VALAR_SetHeroCombiners``` is a Passthrough + Passthrough combiner which will allow individual draw calls to take precedent over the VRS Tier 2 shading rate. These combiners can be used to preserve visual quality for "Hero" art assets or to fix visual corruption from shaders that don't work well with Variable Rate Shading.

```c++
valarDesc.m_baseShadingRate = VALAR_SHADING_RATE_2X2;
valarDesc.m_commandList = commandList;

// ...
// Apply VALAR Mask
// ...

// ...
// Render Frame With Screen Space Shading Rate
// ...

retCode = Intel::VALAR_SetHeroCombiners(valarDesc);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);

// ...
// Render Frame "Hero" Assets
// ...
```

```Intel::VALAR_SetCustomCombiners``` allows the application to set a custom set of combiners if needed. 

```c++
valarDesc.m_baseShadingRate = VALAR_SHADING_RATE_4X4;
valarDesc.m_commandList = commandList;

retCode = Intel::VALAR_SetCustomCombiners(valarDesc, 
    VALAR_SHADING_RATE_COMBINER_MIN, 
    VALAR_SHADING_RATE_COMBINER_MAX);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);
```

If the call to any of these functions is successful a return code ```VALAR_RETURN_CODE_SUCCESS``` is returned. Otherwise one of the following error codes will be returned.

* ```VALAR_RETURN_CODE_NOT_INITIALIZED``` indicates the ```VALAR_DESCRIPTOR``` was not properly initialized.
* ```VALAR_RETURN_CODE_NOT_SUPPORTED``` indicates that the device used to initialize the descriptor does not support VRS Tier 1
* ```VALAR_RETURN_CODE_INVALID_ARGUMENT``` indicates that ```m_commandList``` is ```nullptr```.

## VALAR Debug Overlay

![Alt text](/VALAR/img/VALAR_Screenshot_1080p_Allow4x4_DebugOverlay.png?raw=true "Debug Overlay")

The VALAR API provides a debug overlay that can be used to visualize the VALAR buffer at runtime to aid in debugging. The debug visualization works on both native resolution render targets as well as upscaled render targets from Intel XeSS. To use the debug overlay a ```VALAR_DESCRIPTOR``` must be used with the  ```m_debugOverlay``` field set to true. The ```VALAR_DESCRIPTOR``` must also have a valid ```ID3D12GraphicsCommandList5``` pointer set for the ```m_commandList``` field along with valid resources for the color buffer (```m_colorBuffer```), VRS buffer (```m_valarBuffer```), and SRV/UAV heap (```m_uavHeap```).

The Debug Overlay requires a 2 slot UAV heap; UAV slot 0 contains the VRS Buffer to visualize, while UAV slot 2 will contain a native resolution color UAV or an XeSS upscaled color UAV to draw the overlay on. An upscaled width and height must always be passed in through the descriptor using ```m_upscaledWidth``` and ```m_upscaledHeight```. When using a native resolution color buffer, set ```m_upscaledWidth``` and ```m_upscaledHeight``` equal to ```m_nativeWidth``` and ```m_nativeHeight``` respectively. 

```c++

// ...
// Render Scene
// ...

// ... 
// Generate VALAR Mask
// ... 

// Render the Debug Overlay
D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
uavDesc.Texture2DArray.PlaneSlice = 0;
uavDesc.Texture2DArray.FirstArraySlice = 0;
uavDesc.Texture2DArray.MipSlice = 0;
uavDesc.Texture2DArray.ArraySize = 1;
uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;

// Use UAV Slot 0 for VRS Buffer with format of DXGI_FORMAT_R8_UINT
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_valarDebugHeap->GetCPUDescriptorHandleForHeapStart(), 0, uavDescriptorSize);

    uavDesc.Format = DXGI_FORMAT_R8_UINT;
          
    g_Device->CreateUnorderedAccessView(g_VRSTier2Buffer.GetResource(), nullptr, &uavDesc, uavHandle);
}

// Use UAV Slot 1 for Upscaled Color buffer or Native Color Buffer
if(upscaledOverlay)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_valarDebugHeap->GetCPUDescriptorHandleForHeapStart(), 1, uavDescriptorSize);

    uavDesc.Format = g_UpscaledSceneColorBuffer.GetFormat();

    g_Device->CreateUnorderedAccessView(g_UpscaledSceneColorBuffer.GetResource(), nullptr, &uavDesc, uavHandle);

    valarDesc.m_upscaleHeight = g_UpscaledSceneColorBuffer.GetHeight();
    valarDesc.m_upscaleWidth = g_UpscaledSceneColorBuffer.GetWidth();
}
else
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_valarDebugHeap->GetCPUDescriptorHandleForHeapStart(), 1, uavDescriptorSize);

    uavDesc.Format = g_SceneColorBuffer.GetFormat();

    g_Device->CreateUnorderedAccessView(g_SceneColorBuffer.GetResource(), nullptr, &uavDesc, uavHandle);

    valarDesc.m_upscaleHeight = g_SceneColorBuffer.GetHeight();
    valarDesc.m_upscaleWidth = g_SceneColorBuffer.GetWidth();
}

valarDesc.m_debugOverlay = true;
valarDesc.m_debugGrid = VRS::DebugDrawDrawGrid;
valarDesc.m_bufferHeight = Target.GetHeight();
valarDesc.m_bufferWidth = Target.GetWidth();

// Use the 2 slot Debug Heap
valarDesc.m_uavHeap = m_valarDebugHeap;

Intel::VALAR_RETURN_CODE retCode = Intel::VALAR_DebugOverlay(valarDesc);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);

// ...
// Render GUI
// ...
```

```Intel::VALAR_DebugOverlay``` will return ```VALAR_RETURN_CODE_SUCCESS``` when successfully called. Otherwise the following error codes may be returned on failure.

* ```VALAR_RETURN_CODE_NOT_INITIALIZED``` indicates that ```Intel::VALAR_Initialize``` function failed or was never called.
* ```VALAR_RETURN_CODE_INVALID_DEVICE``` indicates that the opaque descriptors internal device is invalid.
* ```VALAR_RETURN_CODE_NOT_SUPPORTED``` indicates that the device does not support VRS Tier 2
* ```VALAR_RETURN_CODE_INVALID_ARGUMENT``` indicates that the Command List or UAV Heap is invalid.

## Releasing the VALAR API

When the application shuts down it is important to cleanup the internal resources required by the VALAR API. This can be done by calling the ```Intel::VALAR_Release``` function with a valid ```VALAR_DESCRIPTOR``` reference.

```c++
// Release Internal VALAR API Resources
Intel::VALAR_RETURN_CODE retCode = Intel::VALAR_Release(m_valarDescriptor);
assert(retCode == Intel::VALAR_RETURN_CODE_SUCCESS);
```

```Intel::VALAR_Release``` will return ```VALAR_RETURN_CODE_SUCCESS``` when successfully called. Otherwise the following error codes may be returned on failure.

* ```VALAR_RETURN_CODE_NOT_INITIALIZED``` indicates that ```Intel::VALAR_Initialize``` function failed or was never called.

After calling ```Intel::VALAR_Release``` the VALAR descriptor used will no longer be valid and cannot be used unless ```Intel::VALAR_Initialize``` is called first.

## Credits

Many thanks to Lei Yang and his paper on visually lossless motion adaptive shading in games, http://leiy.cc/publications/nas/nas-pacmcgit.pdf.

Thanks to Adam Lake, Aria Kraft, Dietmar Suoch, Daniele Pieroni, Meghan Weicht, Ani Alston, for co-authoring, reviewing and supporting this work as well as many others who helped along the way!

Many thanks to the developers of the following open-source libraries or projects that helped make this project possible.
 * dear imgui (https://github.com/ocornut/imgui)
 * AMD's AOFX (https://github.com/shadercoder/AOFX)
 * fx-gltf by Jesse Yurkovich (https://github.com/jessey-git/fx-gltf)

## References

 * **[Yang, 2019]**: AS/NAS, Visually Lossless Content and Motion Adaptive Shading in Games, Lei Yang, 2019, <http://leiy.cc/publications/nas/nas-pacmcgit.pdf>  
 * **[Weicht, Lake, Kraft, du Bois, 2022]**: Velocity and Luminance Adaptive Rasterization Using VRS Tier 2, Weicht, Lake, Kraft, du Bois, 2022, <https://www.intel.com/content/www/us/en/developer/articles/technical/velocity-luminance-adaptive-rasterization-vrs-tier.html>

## License

Sample and its code provided under MIT license, please see [LICENSE](/LICENSE). All third-party source code provided under their own respective and MIT-compatible Open Source licenses.

Copyright (C) 2023, Intel Corporation  
