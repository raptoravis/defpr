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

#include <random>

#include "Tutorial14_ComputeShader.h"
#include "BasicMath.h"
#include "MapHelper.h"
#include "imgui.h"
#include "ShaderMacroHelper.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial14_ComputeShader();
}

namespace
{

struct ParticleAttribs
{
    float2 f2Pos;
    float2 f2NewPos;

    float2 f2Speed;
    float2 f2NewSpeed;

    float  fSize;
    float  fTemperature;
    int    iNumCollisions;
    float  fPadding0;
};

}

void Tutorial14_ComputeShader::CreateRenderParticlePSO()
{
    PipelineStateDesc PSODesc;
    // Pipeline state name is used by the engine to report issues.
    PSODesc.Name = "Render particles PSO"; 

    // This is a graphics pipeline
    PSODesc.IsComputePipeline = false; 

    // This tutorial will render to a single render target
    PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSODesc.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSODesc.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // Disable back face culling
    PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Disable depth testing
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    auto& BlendDesc = PSODesc.GraphicsPipeline.BlendDesc;
    BlendDesc.RenderTargets[0].BlendEnable = True;
    BlendDesc.RenderTargets[0].SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
    BlendDesc.RenderTargets[0].DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;

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
    // Create particle vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Particle VS";
        ShaderCI.FilePath        = "particle.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    // Create particle pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Particle PS";
        ShaderCI.FilePath        = "particle.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    PSODesc.GraphicsPipeline.pVS = pVS;
    PSODesc.GraphicsPipeline.pPS = pPS;

    // Define variable type that will be used by default
    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_VERTEX, "g_Particles", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    m_pDevice->CreatePipelineState(PSODesc, &m_pRenderParticlePSO);
    m_pRenderParticlePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_Constants);
}

void Tutorial14_ComputeShader::CreateUpdateParticlePSO()
{
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

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro( "THREAD_GROUP_SIZE", m_ThreadGroupSize );
    Macros.Finalize();

    RefCntAutoPtr<IShader> pResetParticleListsCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Reset particle lists CS";
        ShaderCI.FilePath        = "reset_particle_lists.csh";
        ShaderCI.Macros          = Macros;
        m_pDevice->CreateShader(ShaderCI, &pResetParticleListsCS);
    }

    RefCntAutoPtr<IShader> pMoveParticlesCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Move particles CS";
        ShaderCI.FilePath        = "move_particles.csh";
        ShaderCI.Macros          = Macros;
        m_pDevice->CreateShader(ShaderCI, &pMoveParticlesCS);
    }

    RefCntAutoPtr<IShader> pCollideParticlesCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Collide particles CS";
        ShaderCI.FilePath        = "collide_particles.csh";
        ShaderCI.Macros          = Macros;
        m_pDevice->CreateShader(ShaderCI, &pCollideParticlesCS);
    }

    RefCntAutoPtr<IShader> pUpdatedSpeedCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Update particle speed CS";
        ShaderCI.FilePath        = "collide_particles.csh";
        Macros.AddShaderMacro("UPDATE_SPEED", 1);
        ShaderCI.Macros          = Macros;
        m_pDevice->CreateShader(ShaderCI, &pUpdatedSpeedCS);
    }

    PipelineStateDesc PSODesc;
    // This is a compute pipeline
    PSODesc.IsComputePipeline = true;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_COMPUTE, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    };
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);
    
    PSODesc.Name = "Reset particle lists PSO"; 
    PSODesc.ComputePipeline.pCS = pResetParticleListsCS;
    m_pDevice->CreatePipelineState(PSODesc, &m_pResetParticleListsPSO);
    m_pResetParticleListsPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

    PSODesc.Name = "Move particles PSO"; 
    PSODesc.ComputePipeline.pCS = pMoveParticlesCS;
    m_pDevice->CreatePipelineState(PSODesc, &m_pMoveParticlesPSO);
    m_pMoveParticlesPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

    PSODesc.Name = "Collidse particles PSO"; 
    PSODesc.ComputePipeline.pCS = pCollideParticlesCS;
    m_pDevice->CreatePipelineState(PSODesc, &m_pCollideParticlesPSO);
    m_pCollideParticlesPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

    PSODesc.Name = "Update particle speed PSO"; 
    PSODesc.ComputePipeline.pCS = pUpdatedSpeedCS;
    m_pDevice->CreatePipelineState(PSODesc, &m_pUpdateParticleSpeedPSO);
    m_pUpdateParticleSpeedPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);
}

void Tutorial14_ComputeShader::CreateParticleBuffers()
{
    m_pParticleAttribsBuffer.Release();
    m_pParticleListHeadsBuffer.Release();
    m_pParticleListsBuffer.Release();

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Particle attribs buffer";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(ParticleAttribs);
    BuffDesc.uiSizeInBytes     = sizeof(ParticleAttribs) * m_NumParticles;

    std::vector<ParticleAttribs> ParticleData(m_NumParticles);
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<float> pos_distr(-1.f, +1.f);
    std::uniform_real_distribution<float> size_distr(0.5f, 1.f);
    constexpr float fMaxParticleSize = 0.05f;
    float fSize = 0.7f / std::sqrt(static_cast<float>(m_NumParticles));
    fSize = std::min(fMaxParticleSize, fSize);
    for(auto& particle : ParticleData)
    {
        particle.f2NewPos.x   = pos_distr(rd);
        particle.f2NewPos.y   = pos_distr(rd);
        particle.f2NewSpeed.x = pos_distr(rd) * fSize * 5.f;
        particle.f2NewSpeed.y = pos_distr(rd) * fSize * 5.f;
        particle.fSize        = fSize * size_distr(rd);
    }

    BufferData VBData;
    VBData.pData    = ParticleData.data();
    VBData.DataSize = sizeof(ParticleAttribs) * static_cast<Uint32>(ParticleData.size());
    m_pDevice->CreateBuffer(BuffDesc, &VBData, &m_pParticleAttribsBuffer);
    IBufferView* pParticleAttribsBufferSRV = m_pParticleAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    IBufferView* pParticleAttribsBufferUAV = m_pParticleAttribsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);

    BuffDesc.ElementByteStride = sizeof(int);
    BuffDesc.Mode              = BUFFER_MODE_FORMATTED;
    BuffDesc.uiSizeInBytes     = BuffDesc.ElementByteStride * static_cast<Uint32>(m_NumParticles);
    BuffDesc.BindFlags         = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pParticleListHeadsBuffer);
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pParticleListsBuffer);
    RefCntAutoPtr<IBufferView> pParticleListHeadsBufferUAV;
    RefCntAutoPtr<IBufferView> pParticleListsBufferUAV;
    RefCntAutoPtr<IBufferView> pParticleListHeadsBufferSRV;
    RefCntAutoPtr<IBufferView> pParticleListsBufferSRV;
    {
        BufferViewDesc ViewDesc;
        ViewDesc.ViewType = BUFFER_VIEW_UNORDERED_ACCESS;
        ViewDesc.Format.ValueType     = VT_INT32;
        ViewDesc.Format.NumComponents = 1;
        m_pParticleListHeadsBuffer->CreateView(ViewDesc, &pParticleListHeadsBufferUAV);
        m_pParticleListsBuffer->CreateView(ViewDesc, &pParticleListsBufferUAV);

        ViewDesc.ViewType = BUFFER_VIEW_SHADER_RESOURCE;
        m_pParticleListHeadsBuffer->CreateView(ViewDesc, &pParticleListHeadsBufferSRV);
        m_pParticleListsBuffer->CreateView(ViewDesc, &pParticleListsBufferSRV);
    }

    m_pResetParticleListsSRB.Release();
    m_pResetParticleListsPSO->CreateShaderResourceBinding(&m_pResetParticleListsSRB, true);
    m_pResetParticleListsSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferUAV);

    m_pRenderParticleSRB.Release();
    m_pRenderParticlePSO->CreateShaderResourceBinding(&m_pRenderParticleSRB, true);
    m_pRenderParticleSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Particles")->Set(pParticleAttribsBufferSRV);

    m_pMoveParticlesSRB.Release();
    m_pMoveParticlesPSO->CreateShaderResourceBinding(&m_pMoveParticlesSRB, true);
    m_pMoveParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Particles")->Set(pParticleAttribsBufferUAV);
    m_pMoveParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferUAV);
    m_pMoveParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleLists")->Set(pParticleListsBufferUAV);

    m_pCollideParticlesSRB.Release();
    m_pCollideParticlesPSO->CreateShaderResourceBinding(&m_pCollideParticlesSRB, true);
    m_pCollideParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Particles")->Set(pParticleAttribsBufferUAV);
    m_pCollideParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferSRV);
    m_pCollideParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleLists")->Set(pParticleListsBufferSRV);
}

void Tutorial14_ComputeShader::CreateConsantBuffer()
{
    BufferDesc BuffDesc;
    BuffDesc.Name           = "Constants buffer";
    BuffDesc.Usage          = USAGE_DYNAMIC;
    BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    BuffDesc.uiSizeInBytes  = sizeof(float4) * 2;
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Constants);
}

void Tutorial14_ComputeShader::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::InputInt("Num Particles", &m_NumParticles, 100, 1000, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            m_NumParticles = std::min(std::max(m_NumParticles, 100), 100000);
            CreateParticleBuffers();
        }
        ImGui::SliderFloat("Simulation Speed", &m_fSimulationSpeed, 0.1f, 5.f);
    }
    ImGui::End();
}

void Tutorial14_ComputeShader::Initialize(IEngineFactory*   pEngineFactory,
                                          IRenderDevice*    pDevice,
                                          IDeviceContext**  ppContexts,
                                          Uint32            NumDeferredCtx,
                                          ISwapChain*       pSwapChain)
{
    const auto& deviceCaps = pDevice->GetDeviceCaps();
    if (!deviceCaps.bComputeShadersSupported)
    {
        throw std::runtime_error("Compute shaders are required to run this tutorial");
    }

    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    CreateConsantBuffer();
    CreateRenderParticlePSO();
    CreateUpdateParticlePSO();
    CreateParticleBuffers();
}

// Render a frame
void Tutorial14_ComputeShader::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    // Let the engine perform required state transitions
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        struct Constants
        {
            uint   uiNumParticles;
            float  fDeltaTime;
            float  fDummy0;
            float  fDummy1;

            float2 f2Scale;
            int2   i2ParticleGridSize;
        };
        // Map the buffer and write current world-view-projection matrix
        MapHelper<Constants> ConstData(m_pImmediateContext, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD);
        ConstData->uiNumParticles = static_cast<Uint32>(m_NumParticles);
        ConstData->fDeltaTime     = std::min(m_fTimeDelta, 1.f/60.f) * m_fSimulationSpeed;

        float AspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
        float2 f2Scale = float2(std::sqrt(1.f / AspectRatio), std::sqrt(AspectRatio));
        ConstData->f2Scale = f2Scale;

        int iParticleGridWidth = static_cast<int>(std::sqrt(static_cast<float>(m_NumParticles)) / f2Scale.x);
        ConstData->i2ParticleGridSize.x = iParticleGridWidth;
        ConstData->i2ParticleGridSize.y = m_NumParticles / iParticleGridWidth;
    }

    DispatchComputeAttribs DispatAttribs;
    DispatAttribs.ThreadGroupCountX = (m_NumParticles + m_ThreadGroupSize-1) / m_ThreadGroupSize;

    m_pImmediateContext->SetPipelineState(m_pResetParticleListsPSO);
    m_pImmediateContext->CommitShaderResources(m_pResetParticleListsSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(DispatAttribs);

    m_pImmediateContext->SetPipelineState(m_pMoveParticlesPSO);
    m_pImmediateContext->CommitShaderResources(m_pMoveParticlesSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(DispatAttribs);

    m_pImmediateContext->SetPipelineState(m_pCollideParticlesPSO);
    m_pImmediateContext->CommitShaderResources(m_pCollideParticlesSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(DispatAttribs);

    m_pImmediateContext->SetPipelineState(m_pUpdateParticleSpeedPSO);
    // Use the same SRB
    m_pImmediateContext->CommitShaderResources(m_pCollideParticlesSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(DispatAttribs);

    m_pImmediateContext->SetPipelineState(m_pRenderParticlePSO);
    m_pImmediateContext->CommitShaderResources(m_pRenderParticleSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices  = 4;
    drawAttrs.NumInstances = static_cast<Uint32>(m_NumParticles);
    m_pImmediateContext->Draw(drawAttrs);
}

void Tutorial14_ComputeShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    m_fTimeDelta = static_cast<float>(ElapsedTime);
}

}
