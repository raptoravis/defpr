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

#include "Tutorial08_Tessellation.h"
#include "MapHelper.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ShaderMacroHelper.h"
#include "imgui.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial08_Tessellation();
}

namespace
{
    struct GlobalConstants
    {
        unsigned int NumHorzBlocks; // Number of blocks along the horizontal edge
        unsigned int NumVertBlocks; // Number of blocks along the horizontal edge
        float fNumHorzBlocks;
        float fNumVertBlocks;

        float fBlockSize;
        float LengthScale;
        float HeightScale;
        float LineWidth;

        float TessDensity;
        int AdaptiveTessellation;
        float2 Dummy2;

        float4x4 WorldView;
        float4x4 WorldViewProj;
        float4 ViewportSize;
    };
}

void Tutorial08_Tessellation::GetEngineInitializationAttribs(DeviceType         DevType,
                                                             EngineCreateInfo&  Attribs)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs);
#if VULKAN_SUPPORTED
    if(DevType == DeviceType::Vulkan)
    {
        auto& VkAttrs = static_cast<EngineVkCreateInfo&>(Attribs);
        VkAttrs.EnabledFeatures.geometryShader     = true;
        VkAttrs.EnabledFeatures.tessellationShader = true;
    }
#endif
}

void Tutorial08_Tessellation::CreatePipelineStates()
{
    const bool bWireframeSupported = m_pDevice->GetDeviceCaps().bGeometryShadersSupported;

    ShaderMacroHelper MacroHelper;

    // Pipeline state object encompasses configuration of all GPU stages

    PipelineStateDesc PSODesc;
    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSODesc.Name = "Terrain PSO"; 

    // This is a graphics pipeline
    PSODesc.IsComputePipeline = false; 

    // This tutorial will render to a single render target
    PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSODesc.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSODesc.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology type defines what kind of primitives will be rendered by this pipeline state
    PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
    // Cull back faces. For some reason, in OpenGL the order is reversed
    PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = m_pDevice->GetDeviceCaps().IsGLDevice() ? CULL_MODE_FRONT : CULL_MODE_BACK;
    // Enable depth testing
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    // Create dynamic uniform buffer that will store shader constants
    CreateUniformBuffer(m_pDevice, sizeof(GlobalConstants), "Global shader constants CB", &m_ShaderConstants);

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
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "TerrainVS";
        ShaderCI.Desc.Name       = "Terrain VS";
        ShaderCI.FilePath        = "terrain.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }


    // Create a geometry shader
    RefCntAutoPtr<IShader> pGS;
    if (bWireframeSupported)
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_GEOMETRY;
        ShaderCI.EntryPoint      = "TerrainGS";
        ShaderCI.Desc.Name       = "Terrain GS";
        ShaderCI.FilePath        = "terrain.gsh";
        m_pDevice->CreateShader(ShaderCI, &pGS);
    }

    // Create a hull shader
    RefCntAutoPtr<IShader> pHS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_HULL;
        ShaderCI.EntryPoint      = "TerrainHS";
        ShaderCI.Desc.Name       = "Terrain HS";
        ShaderCI.FilePath        = "terrain.hsh";
        MacroHelper.AddShaderMacro("BLOCK_SIZE", m_BlockSize);
        ShaderCI.Macros          = MacroHelper;
        m_pDevice->CreateShader(ShaderCI, &pHS);
    }

    // Create a domain shader
    RefCntAutoPtr<IShader> pDS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_DOMAIN;
        ShaderCI.EntryPoint      = "TerrainDS";
        ShaderCI.Desc.Name       = "Terrain DS";
        ShaderCI.FilePath        = "terrain.dsh";
        ShaderCI.Macros          = nullptr;
        m_pDevice->CreateShader(ShaderCI, &pDS);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS, pWirePS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "TerrainPS";
        ShaderCI.Desc.Name       = "Terrain PS";
        ShaderCI.FilePath        = "terrain.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);

        if (bWireframeSupported)
        {
            ShaderCI.EntryPoint = "WireTerrainPS";
            ShaderCI.Desc.Name  = "Wireframe Terrain PS";
            ShaderCI.FilePath   = "terrain_wire.psh";
            m_pDevice->CreateShader(ShaderCI, &pWirePS);
        }
    }

    PSODesc.GraphicsPipeline.pVS = pVS;
    PSODesc.GraphicsPipeline.pHS = pHS;
    PSODesc.GraphicsPipeline.pDS = pDS;
    PSODesc.GraphicsPipeline.pPS = pPS;

    // Define variable type that will be used by default
    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_HULL | SHADER_TYPE_DOMAIN,  "g_HeightMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL,                      "g_Texture",   SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // Define static sampler for g_HeightMap and g_Texture. Static samplers should be used whenever possible
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    StaticSamplerDesc StaticSamplers[] = 
    {
        {SHADER_TYPE_HULL | SHADER_TYPE_DOMAIN, "g_HeightMap", SamLinearClampDesc},
        {SHADER_TYPE_PIXEL,                     "g_Texture",   SamLinearClampDesc}
    };
    PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
    PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);

    m_pDevice->CreatePipelineState(PSODesc, &m_pPSO[0]);

    if (bWireframeSupported)
    {
        PSODesc.GraphicsPipeline.pGS = pGS;
        PSODesc.GraphicsPipeline.pPS = pWirePS;
        m_pDevice->CreatePipelineState(PSODesc, &m_pPSO[1]);
    }
        
    for (Uint32 i = 0; i < _countof(m_pPSO); ++i)
    {
        if (m_pPSO[i])
        {
            m_pPSO[i]->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")->Set(m_ShaderConstants);
            m_pPSO[i]->GetStaticVariableByName(SHADER_TYPE_HULL,   "HSConstants")->Set(m_ShaderConstants);
            m_pPSO[i]->GetStaticVariableByName(SHADER_TYPE_DOMAIN, "DSConstants")->Set(m_ShaderConstants);
        }
    }
    if (m_pPSO[1])
    {
        m_pPSO[1]->GetStaticVariableByName(SHADER_TYPE_GEOMETRY, "GSConstants")->Set(m_ShaderConstants);
        m_pPSO[1]->GetStaticVariableByName(SHADER_TYPE_PIXEL,    "PSConstants")->Set(m_ShaderConstants);
    }
}

void Tutorial08_Tessellation::LoadTextures()
{
    {
        // Load texture
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = false;
        loadInfo.Name = "Terrain height map";
        RefCntAutoPtr<ITexture> HeightMap;
        CreateTextureFromFile("ps_height_1k.png", loadInfo, m_pDevice, &HeightMap);
        const auto HMDesc = HeightMap->GetDesc();
        m_HeightMapWidth  = HMDesc.Width;
        m_HeightMapHeight = HMDesc.Height;
        // Get shader resource view from the texture
        m_HeightMapSRV = HeightMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }

    {
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        loadInfo.Name = "Terrain color map";
        RefCntAutoPtr<ITexture> ColorMap;
        CreateTextureFromFile("ps_texture_2k.png", loadInfo, m_pDevice, &ColorMap);
        // Get shader resource view from the texture
        m_ColorMapSRV = ColorMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    for (size_t i=0; i < _countof(m_pPSO); ++i)
    {
        if (m_pPSO[i])
        {
            m_pPSO[i]->CreateShaderResourceBinding(&m_SRB[i], true);
            // Set texture SRV in the SRB
            m_SRB[i]->GetVariableByName(SHADER_TYPE_PIXEL,  "g_Texture")->Set(m_ColorMapSRV);
            m_SRB[i]->GetVariableByName(SHADER_TYPE_DOMAIN, "g_HeightMap")->Set(m_HeightMapSRV);
            m_SRB[i]->GetVariableByName(SHADER_TYPE_HULL,   "g_HeightMap")->Set(m_HeightMapSRV);
        }
    }
}

void Tutorial08_Tessellation::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Checkbox("Animate", &m_Animate);
        ImGui::Checkbox("Adaptive tessellation", &m_AdaptiveTessellation);
        if (m_pPSO[1])
            ImGui::Checkbox("Wireframe", &m_Wireframe);
        ImGui::SliderFloat("Tess density", &m_TessDensity, 1.f, 32.f);
        ImGui::SliderFloat("Distance", &m_Distance, 1.f, 20.f);
    }
    ImGui::End();
}

void Tutorial08_Tessellation::Initialize(IEngineFactory*   pEngineFactory,
                                         IRenderDevice*    pDevice,
                                         IDeviceContext**  ppContexts,
                                         Uint32            NumDeferredCtx,
                                         ISwapChain*       pSwapChain)
{
    const auto& deviceCaps = pDevice->GetDeviceCaps();
    if (!deviceCaps.bTessellationSupported)
    {
        throw std::runtime_error("Hardware tessellation is not supported");
    }

    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    CreatePipelineStates();
    LoadTextures();
}

// Render a frame
void Tutorial08_Tessellation::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    unsigned int NumHorzBlocks = m_HeightMapWidth / m_BlockSize;
    unsigned int NumVertBlocks = m_HeightMapHeight / m_BlockSize;
    {
        // Map the buffer and write rendering data
        MapHelper<GlobalConstants> Consts(m_pImmediateContext, m_ShaderConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        Consts->fBlockSize = static_cast<float>(m_BlockSize);
        Consts->NumHorzBlocks = NumHorzBlocks;
        Consts->NumVertBlocks = NumVertBlocks;
        Consts->fNumHorzBlocks = static_cast<float>(NumHorzBlocks);
        Consts->fNumVertBlocks = static_cast<float>(NumVertBlocks);

        Consts->LengthScale = 10.f;
        Consts->HeightScale = Consts->LengthScale / 25.f;

        Consts->WorldView     = m_WorldViewMatrix.Transpose();
        Consts->WorldViewProj = m_WorldViewProjMatrix.Transpose();

        Consts->TessDensity = m_TessDensity;
        Consts->AdaptiveTessellation = m_AdaptiveTessellation ? 1 : 0;
        
        const auto &SCDesc = m_pSwapChain->GetDesc();
        Consts->ViewportSize = float4(static_cast<float>(SCDesc.Width), static_cast<float>(SCDesc.Height), 1.f/static_cast<float>(SCDesc.Width), 1.f/static_cast<float>(SCDesc.Height));
        
        Consts->LineWidth = 3.0f;
    }


    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO[m_Wireframe ? 1 : 0]);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode 
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_SRB[m_Wireframe ? 1 : 0], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs;
    DrawAttrs.NumVertices = NumHorzBlocks * NumVertBlocks;
    DrawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->Draw(DrawAttrs);
}

void Tutorial08_Tessellation::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    const bool IsGL = m_pDevice->GetDeviceCaps().IsGLDevice();

    // Set world view matrix
    if (m_Animate)
    {
        m_RotationAngle += static_cast<float>(ElapsedTime) * 0.2f;
        if(m_RotationAngle > PI_F*2.f)
            m_RotationAngle -= PI_F*2.f;
    }

    m_WorldViewMatrix = float4x4::RotationY(m_RotationAngle) * float4x4::RotationX(-PI_F*0.1f) * float4x4::Translation(0.f, 0.0f, m_Distance);
    float NearPlane = 0.1f;
    float FarPlane = 1000.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = float4x4::Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsGL);
    // Compute world-view-projection matrix
    m_WorldViewProjMatrix = m_WorldViewMatrix * Proj;
}

}
