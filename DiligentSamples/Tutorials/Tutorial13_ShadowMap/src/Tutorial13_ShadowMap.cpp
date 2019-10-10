/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include <vector>

#include "Tutorial13_ShadowMap.h"
#include "MapHelper.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "imgui.h"
#include "ImGuiUtils.h"
#include "imGuIZMO.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial13_ShadowMap();
}
    
void Tutorial13_ShadowMap::GetEngineInitializationAttribs(DeviceType         DevType,
                                                          EngineCreateInfo&  Attribs)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs);
#if VULKAN_SUPPORTED
    if(DevType == DeviceType::Vulkan)
    {
        auto& VkAttrs = static_cast<EngineVkCreateInfo&>(Attribs);
        VkAttrs.EnabledFeatures.depthClamp = true;
    }
#endif
}

void Tutorial13_ShadowMap::CreateCubePSO()
{
    // Pipeline state object encompasses configuration of all GPU stages
    
    PipelineStateDesc PSODesc;
    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSODesc.Name = "Cube PSO";
    
    // This is a graphics pipeline
    PSODesc.IsComputePipeline = false;
    
    // This tutorial renders to a single render target
    PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSODesc.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSODesc.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    
    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;
    
    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create cube vertex shader
    RefCntAutoPtr<IShader> pCubeVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pCubeVS);
    }
    
    // Create cube pixel shader
    RefCntAutoPtr<IShader> pCubePS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pCubePS);
    }
    
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False},
        // Attribute 2 - normal
        LayoutElement{2, 0, 3, VT_FLOAT32, False},
    };
    
    PSODesc.GraphicsPipeline.pVS = pCubeVS;
    PSODesc.GraphicsPipeline.pPS = pCubePS;
    PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSODesc.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);
    
    // Define variable type that will be used by default
    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);
    
    // Define static sampler for g_Texture. Static samplers should be used whenever possible
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    StaticSamplerDesc StaticSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
    };
    PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
    PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);
    
    m_pDevice->CreatePipelineState(PSODesc, &m_pCubePSO);
    
    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    
    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pCubePSO->CreateShaderResourceBinding(&m_CubeSRB, true);
    
    
    // Create shadow vertex shader
    RefCntAutoPtr<IShader> pShadowVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube Shadow VS";
        ShaderCI.FilePath        = "cube_shadow.vsh";
        m_pDevice->CreateShader(ShaderCI, &pShadowVS);
    }
    
    PSODesc.Name                              = "Cube shadow PSO";
    // Shadow pass doesn't use any render target outputs
    PSODesc.GraphicsPipeline.NumRenderTargets = 0;
    PSODesc.GraphicsPipeline.RTVFormats[0]    = TEX_FORMAT_UNKNOWN;
    // The DSV format is the shadow map format
    PSODesc.GraphicsPipeline.DSVFormat        = m_ShadowMapFormat;
    PSODesc.GraphicsPipeline.pVS              = pShadowVS;
    // We don't use pixel shader as we are only interested in populating the depth buffer
    PSODesc.GraphicsPipeline.pPS              = nullptr;
    PSODesc.ResourceLayout.Variables          = nullptr;
    PSODesc.ResourceLayout.NumVariables       = 0;
    PSODesc.ResourceLayout.StaticSamplers     = nullptr;
    PSODesc.ResourceLayout.NumStaticSamplers  = 0;
    
    // Disable depth clipping to render objects that are closer than near
    // clipping plane. This is not required for this tutorial, but real applications
    // will most likely want to do this.
    PSODesc.GraphicsPipeline.RasterizerDesc.DepthClipEnable = False;
    
    m_pDevice->CreatePipelineState(PSODesc, &m_pCubeShadowPSO);
    m_pCubeShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pCubeShadowPSO->CreateShaderResourceBinding(&m_CubeShadowSRB, true);
}
 
void Tutorial13_ShadowMap::CreatePlanePSO()
{
    PipelineStateDesc PSODesc;
    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSODesc.Name = "Plane PSO";
    
    // This is a graphics pipeline
    PSODesc.IsComputePipeline = false;
    
    // This tutorial renders to a single render target
    PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSODesc.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSODesc.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // No cull
    PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Enable depth testing
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    
    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;
    
    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create plane vertex shader
    RefCntAutoPtr<IShader> pPlaneVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Plane VS";
        ShaderCI.FilePath        = "plane.vsh";
        m_pDevice->CreateShader(ShaderCI, &pPlaneVS);
    }
    
    // Create plane pixel shader
    RefCntAutoPtr<IShader> pPlanePS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Plane PS";
        ShaderCI.FilePath        = "plane.psh";
        m_pDevice->CreateShader(ShaderCI, &pPlanePS);
    }
    
    PSODesc.GraphicsPipeline.pVS = pPlaneVS;
    PSODesc.GraphicsPipeline.pPS = pPlanePS;
    
    // Define variable type that will be used by default
    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);
    
    // Define static comparison sampler for g_ShadowMap. Static samplers should be used whenever possible
    SamplerDesc ComparsionSampler;
    ComparsionSampler.ComparisonFunc = COMPARISON_FUNC_LESS; 
    ComparsionSampler.MinFilter      = FILTER_TYPE_COMPARISON_LINEAR;
    ComparsionSampler.MagFilter      = FILTER_TYPE_COMPARISON_LINEAR;
    ComparsionSampler.MipFilter      = FILTER_TYPE_COMPARISON_LINEAR;
    StaticSamplerDesc StaticSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", ComparsionSampler}
    };
    PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
    PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);
    
    m_pDevice->CreatePipelineState(PSODesc, &m_pPlanePSO);
    
    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
}

void Tutorial13_ShadowMap::CreateShadowMapVisPSO()
{
    PipelineStateDesc PSODesc;
    PSODesc.Name = "Shadow Map Vis PSO";
    
    // This is a graphics pipeline
    PSODesc.IsComputePipeline = false;
    
    // This tutorial renders to a single render target
    PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSODesc.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSODesc.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // No cull
    PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Disable depth testing
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    
    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;
    
    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create shadow map visualization vertex shader
    RefCntAutoPtr<IShader> pShadowMapVisVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Shadow Map Vis VS";
        ShaderCI.FilePath        = "shadow_map_vis.vsh";
        m_pDevice->CreateShader(ShaderCI, &pShadowMapVisVS);
    }
    
    // Create shadow map visualization pixel shader
    RefCntAutoPtr<IShader> pShadowMapVisPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Shadow Map Vis PS";
        ShaderCI.FilePath        = "shadow_map_vis.psh";
        m_pDevice->CreateShader(ShaderCI, &pShadowMapVisPS);
    }
    
    PSODesc.GraphicsPipeline.pVS = pShadowMapVisVS;
    PSODesc.GraphicsPipeline.pPS = pShadowMapVisPS;
    
    // Define variable type that will be used by default
    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    StaticSamplerDesc StaticSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SamLinearClampDesc}
    };
    PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
    PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);
    
    m_pDevice->CreatePipelineState(PSODesc, &m_pShadowMapVisPSO);
}

void Tutorial13_ShadowMap::CreateVertexBuffer(std::vector<StateTransitionDesc>& Barriers)
{
    // Layout of this structure matches the one we defined in pipeline state
    struct Vertex
    {
        float3 pos;
        float2 uv;
        float3 normal;
    };
    
    // Cube vertices
    
    //      (-1,+1,+1)________________(+1,+1,+1)
    //               /|              /|
    //              / |             / |
    //             /  |            /  |
    //            /   |           /   |
    //(-1,-1,+1) /____|__________/(+1,-1,+1)
    //           |    |__________|____|
    //           |   /(-1,+1,-1) |    /(+1,+1,-1)
    //           |  /            |   /
    //           | /             |  /
    //           |/              | /
    //           /_______________|/
    //        (-1,-1,-1)       (+1,-1,-1)
    //
    
    Vertex CubeVerts[] =
    {
        {float3(-1,-1,-1), float2(0,1), float3(0, 0, -1)},
        {float3(-1,+1,-1), float2(0,0), float3(0, 0, -1)},
        {float3(+1,+1,-1), float2(1,0), float3(0, 0, -1)},
        {float3(+1,-1,-1), float2(1,1), float3(0, 0, -1)},
        
        {float3(-1,-1,-1), float2(0,1), float3(0, -1, 0)},
        {float3(-1,-1,+1), float2(0,0), float3(0, -1, 0)},
        {float3(+1,-1,+1), float2(1,0), float3(0, -1, 0)},
        {float3(+1,-1,-1), float2(1,1), float3(0, -1, 0)},
        
        {float3(+1,-1,-1), float2(0,1), float3(+1, 0, 0)},
        {float3(+1,-1,+1), float2(1,1), float3(+1, 0, 0)},
        {float3(+1,+1,+1), float2(1,0), float3(+1, 0, 0)},
        {float3(+1,+1,-1), float2(0,0), float3(+1, 0, 0)},
        
        {float3(+1,+1,-1), float2(0,1), float3(0, +1, 0)},
        {float3(+1,+1,+1), float2(0,0), float3(0, +1, 0)},
        {float3(-1,+1,+1), float2(1,0), float3(0, +1, 0)},
        {float3(-1,+1,-1), float2(1,1), float3(0, +1, 0)},
        
        {float3(-1,+1,-1), float2(1,0), float3(-1, 0, 0)},
        {float3(-1,+1,+1), float2(0,0), float3(-1, 0, 0)},
        {float3(-1,-1,+1), float2(0,1), float3(-1, 0, 0)},
        {float3(-1,-1,-1), float2(1,1), float3(-1, 0, 0)},
        
        {float3(-1,-1,+1), float2(1,1), float3(0, 0, +1)},
        {float3(+1,-1,+1), float2(0,1), float3(0, 0, +1)},
        {float3(+1,+1,+1), float2(0,0), float3(0, 0, +1)},
        {float3(-1,+1,+1), float2(1,0), float3(0, 0, +1)}
    };

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name          = "Cube vertex buffer";
    VertBuffDesc.Usage         = USAGE_STATIC;
    VertBuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
    VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
    BufferData VBData;
    VBData.pData    = CubeVerts;
    VBData.DataSize = sizeof(CubeVerts);
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);

    Barriers.emplace_back(m_CubeVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true);
}
    
void Tutorial13_ShadowMap::CreateIndexBuffer(std::vector<StateTransitionDesc>& Barriers)
{
    Uint32 Indices[] =
    {
        2,0,1,    2,3,0,
        4,6,5,    4,7,6,
        8,10,9,   8,11,10,
        12,14,13, 12,15,14,
        16,18,17, 16,19,18,
        20,21,22, 20,22,23
    };
    
    BufferDesc IndBuffDesc;
    IndBuffDesc.Name          = "Cube index buffer";
    IndBuffDesc.Usage         = USAGE_STATIC;
    IndBuffDesc.BindFlags     = BIND_INDEX_BUFFER;
    IndBuffDesc.uiSizeInBytes = sizeof(Indices);
    BufferData IBData;
    IBData.pData    = Indices;
    IBData.DataSize = sizeof(Indices);
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);

    Barriers.emplace_back(m_CubeIndexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, true);
}
    
void Tutorial13_ShadowMap::LoadTexture(std::vector<StateTransitionDesc>& Barriers)
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> Tex;
    CreateTextureFromFile("DGLogo.png", loadInfo, m_pDevice, &Tex);
    // Get shader resource view from the texture
    m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    
    // Set texture SRV in the SRB
    m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
    Barriers.emplace_back(Tex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true);
}
    
void Tutorial13_ShadowMap::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        constexpr int MinShadowMapSize = 256;
        int ShadowMapComboId = 0;
        while((MinShadowMapSize << ShadowMapComboId) != static_cast<int>(m_ShadowMapSize))
            ++ShadowMapComboId;
        if (ImGui::Combo("Shadow map size", &ShadowMapComboId, "256\0" "512\0" "1024\0\0"))
        {
            m_ShadowMapSize = MinShadowMapSize << ShadowMapComboId;
            CreateShadowMap();
        }
        ImGui::gizmo3D("##LightDirection", m_LightDirection, ImGui::GetTextLineHeight() * 10);
    }
    ImGui::End();   
}
    
void Tutorial13_ShadowMap::Initialize(IEngineFactory*  pEngineFactory,
                                      IRenderDevice*   pDevice,
                                      IDeviceContext** ppContexts,
                                      Uint32           NumDeferredCtx,
                                      ISwapChain*      pSwapChain)
{
    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    std::vector<StateTransitionDesc> Barriers;
    // Create dynamic uniform buffer that will store our transformation matrices
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(pDevice, sizeof(float4x4) * 2 + sizeof(float4), "VS constants CB", &m_VSConstants);
    Barriers.emplace_back(m_VSConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true);
    
    CreateCubePSO();
    CreatePlanePSO();
    CreateShadowMapVisPSO();
    CreateVertexBuffer(Barriers);
    CreateIndexBuffer(Barriers);
    LoadTexture(Barriers);
    CreateShadowMap();

    m_pImmediateContext->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());
}

void Tutorial13_ShadowMap::CreateShadowMap()
{
    TextureDesc SMDesc;
    SMDesc.Name      = "Shadow map";
    SMDesc.Type      = RESOURCE_DIM_TEX_2D;
    SMDesc.Width     = m_ShadowMapSize;
    SMDesc.Height    = m_ShadowMapSize;
    SMDesc.Format    = m_ShadowMapFormat;
    SMDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    RefCntAutoPtr<ITexture> ShadowMap;
    m_pDevice->CreateTexture(SMDesc, nullptr, &ShadowMap);
    m_ShadowMapSRV = ShadowMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_ShadowMapDSV = ShadowMap->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    // Create SRBs that use shadow map as mutable variable
    m_PlaneSRB.Release();
    m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
    m_PlaneSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);

    m_ShadowMapVisSRB.Release();
    m_pShadowMapVisPSO->CreateShaderResourceBinding(&m_ShadowMapVisSRB, true);
    m_ShadowMapVisSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);
}
    
void Tutorial13_ShadowMap::RenderShadowMap()
{
    float3 f3LightSpaceX, f3LightSpaceY, f3LightSpaceZ;
    f3LightSpaceZ = normalize(m_LightDirection);
    
    auto min_cmp = std::min(std::min(std::abs(m_LightDirection.x), std::abs(m_LightDirection.y)), std::abs(m_LightDirection.z));
    if (min_cmp == std::abs(m_LightDirection.x))
        f3LightSpaceX =  float3(1, 0, 0);
    else if (min_cmp == std::abs(m_LightDirection.y))
        f3LightSpaceX =  float3(0, 1, 0);
    else
        f3LightSpaceX =  float3(0, 0, 1);
    
    f3LightSpaceY = cross(f3LightSpaceZ, f3LightSpaceX);
    f3LightSpaceX = cross(f3LightSpaceY, f3LightSpaceZ);
    f3LightSpaceX = normalize(f3LightSpaceX);
    f3LightSpaceY = normalize(f3LightSpaceY);
    
    float4x4 WorldToLightViewSpaceMatr = float4x4::ViewFromBasis(f3LightSpaceX, f3LightSpaceY, f3LightSpaceZ);
    
    // For this tutorial we know that the scene center is at (0,0,0).
    // Real applications will want to compute tight bounds

    float3 f3SceneCenter = float3(0, 0, 0);
    float SceneRadius = std::sqrt(3.f);
    float3 f3MinXYZ = f3SceneCenter - float3(SceneRadius, SceneRadius, SceneRadius);
    float3 f3MaxXYZ = f3SceneCenter + float3(SceneRadius, SceneRadius, SceneRadius*5);
    float3 f3SceneExtent = f3MaxXYZ - f3MinXYZ;

    const auto& DevCaps = m_pDevice->GetDeviceCaps();
    const bool IsGL = DevCaps.IsGLDevice();
    float4 f4LightSpaceScale;
    f4LightSpaceScale.x =  2.f / f3SceneExtent.x;
    f4LightSpaceScale.y =  2.f / f3SceneExtent.y;
    f4LightSpaceScale.z =  (IsGL ? 2.f : 1.f) / f3SceneExtent.z;
    // Apply bias to shift the extent to [-1,1]x[-1,1]x[0,1] for DX or to [-1,1]x[-1,1]x[-1,1] for GL
    // Find bias such that f3MinXYZ -> (-1,-1,0) for DX or (-1,-1,-1) for GL
    float4 f4LightSpaceScaledBias;
    f4LightSpaceScaledBias.x = -f3MinXYZ.x * f4LightSpaceScale.x - 1.f;
    f4LightSpaceScaledBias.y = -f3MinXYZ.y * f4LightSpaceScale.y - 1.f;
    f4LightSpaceScaledBias.z = -f3MinXYZ.z * f4LightSpaceScale.z + (IsGL ? -1.f : 0.f);
    
    float4x4 ScaleMatrix = float4x4::Scale(f4LightSpaceScale.x, f4LightSpaceScale.y, f4LightSpaceScale.z);
    float4x4 ScaledBiasMatrix = float4x4::Translation(f4LightSpaceScaledBias.x, f4LightSpaceScaledBias.y, f4LightSpaceScaledBias.z);
    
    // Note: bias is applied after scaling!
    float4x4 ShadowProjMatr = ScaleMatrix * ScaledBiasMatrix;
    
    // Adjust the world to light space transformation matrix
    float4x4 WorldToLightProjSpaceMatr = WorldToLightViewSpaceMatr * ShadowProjMatr;
    
    const auto& NDCAttribs = DevCaps.GetNDCAttribs();
    float4x4 ProjToUVScale = float4x4::Scale(0.5f, NDCAttribs.YtoVScale, NDCAttribs.ZtoDepthScale);
    float4x4 ProjToUVBias  = float4x4::Translation(0.5f, 0.5f, NDCAttribs.GetZtoDepthBias());
    
    m_WorldToShadowMapUVDepthMatr = WorldToLightProjSpaceMatr * ProjToUVScale * ProjToUVBias;

    RenderCube(WorldToLightProjSpaceMatr, true);
}

void Tutorial13_ShadowMap::RenderCube(const float4x4& CameraViewProj, bool IsShadowPass)
{
    // Update constant buffer
    {
        struct Constants
        {
            float4x4 WorldViewProj;
            float4x4 NormalTranform;
            float4   LightDirection;
        };
        // Map the buffer and write current world-view-projection matrix
        MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->WorldViewProj = (m_CubeWorldMatrix * CameraViewProj).Transpose();
        auto NormalMatrix = m_CubeWorldMatrix.RemoveTranslation().Inverse();
        // We need to do inverse-transpose, but we also need to transpose the matrix
        // before writing it to the buffer
        CBConstants->NormalTranform = NormalMatrix;
        CBConstants->LightDirection = m_LightDirection;
    }

    // Bind vertex buffer
    Uint32 offset = 0;
    IBuffer* pBuffs[] = {m_CubeVertexBuffer};
    // Note that since resouces have been explicitly transitioned to required states, we use RESOURCE_STATE_TRANSITION_MODE_VERIFY flag
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    // Set pipeline state and commit resources
    if (IsShadowPass)
    {
        m_pImmediateContext->SetPipelineState(m_pCubeShadowPSO);
        m_pImmediateContext->CommitShaderResources(m_CubeShadowSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    }
    else
    {
        m_pImmediateContext->SetPipelineState(m_pCubePSO);
        m_pImmediateContext->CommitShaderResources(m_CubeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    }    

    DrawAttribs DrawAttrs(36, VT_UINT32, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
}

void Tutorial13_ShadowMap::RenderPlane()
{
    {
        struct Constants
        {
            float4x4 CameraViewProj;
            float4x4 WorldToShadowMapUVDepth;
            float4   LightDirection;
        };
        MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->CameraViewProj          = m_CameraViewProjMatrix.Transpose();
        CBConstants->WorldToShadowMapUVDepth = m_WorldToShadowMapUVDepthMatr.Transpose();
        CBConstants->LightDirection          = m_LightDirection;
    }

    m_pImmediateContext->SetPipelineState(m_pPlanePSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode 
    // makes sure that resources are transitioned to required states.
    // Note that Vulkan requires shadow map to be transitioned to DEPTH_READ state, not SHADER_RESOURCE
    m_pImmediateContext->CommitShaderResources(m_PlaneSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
}

void Tutorial13_ShadowMap::RenderShadowMapVis()
{
    m_pImmediateContext->SetPipelineState(m_pShadowMapVisPSO);
    m_pImmediateContext->CommitShaderResources(m_ShadowMapVisSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
}

// Render a frame
void Tutorial13_ShadowMap::Render()
{
    // Render shadow map
    m_pImmediateContext->SetRenderTargets(0, nullptr, m_ShadowMapDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_ShadowMapDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    RenderShadowMap();
    
    // Bind main back buffer
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    RenderCube(m_CameraViewProjMatrix, false);
    RenderPlane();
    RenderShadowMapVis();
}

void Tutorial13_ShadowMap::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    const bool IsGL = m_pDevice->GetDeviceCaps().IsGLDevice();

    // Animate the cube
    m_CubeWorldMatrix = float4x4::RotationY( static_cast<float>(CurrTime) * 1.0f);

    float4x4 CameraView = float4x4::Translation(0.f, -5.0f, -10.0f) * float4x4::RotationY(PI_F) * float4x4::RotationX(-PI_F * 0.2);
    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = float4x4::Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsGL);
    // Compute camera view-projection matrix
    m_CameraViewProjMatrix = CameraView * Proj;
}

}
