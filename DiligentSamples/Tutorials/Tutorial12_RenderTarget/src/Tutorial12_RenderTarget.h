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

#pragma once 

#include "SampleBase.h"
#include "BasicMath.h"

namespace Diligent
{

class Tutorial12_RenderTarget final : public SampleBase
{
public:
    virtual void Initialize(IEngineFactory*  pEngineFactory,
                            IRenderDevice*   pDevice, 
                            IDeviceContext** ppContexts, 
                            Uint32           NumDeferredCtx, 
                            ISwapChain*      pSwapChain)override final;
    virtual void Render()override final;
    virtual void Update(double CurrTime, double ElapsedTime)override final;
    virtual const Char* GetSampleName()const override final{return "Tutorial12: Render Target";}
    virtual void WindowResize(Uint32 Width, Uint32 Height)override final;

private:
    void CreateCubePSO();
    void CreateRenderTargetPSO();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void LoadTexture();

    static constexpr TEXTURE_FORMAT       RenderTargetFormat = TEX_FORMAT_RGBA8_UNORM;
    static constexpr TEXTURE_FORMAT       DepthBufferFormat  = TEX_FORMAT_D32_FLOAT;
    // Cube resources
    RefCntAutoPtr<IPipelineState>         m_pCubePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pCubeSRB;
    RefCntAutoPtr<IBuffer>                m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeVSConstants;
    RefCntAutoPtr<ITextureView>           m_CubeTextureSRV;

    // Offscreen render target and depth-stencil
    RefCntAutoPtr<ITextureView>           m_pColorRTV;
    RefCntAutoPtr<ITextureView>           m_pDepthDSV;

    RefCntAutoPtr<IBuffer>                m_RTPSConstants;
    RefCntAutoPtr<IPipelineState>         m_pRTPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pRTSRB;
    float4x4                              m_WorldViewProjMatrix;
    float                                 m_fCurrentTime = 0.f;
};

}
