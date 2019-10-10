/*     Copyright 2015-2018 Egor Yusov
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

#include <memory>
#include <iostream>

#include <GL/glx.h>
#include <GL/gl.h>

#if VULKAN_SUPPORTED
#include <xcb/xcb.h>
#endif

// Undef symbols defined by XLib
#ifdef Bool
#   undef Bool
#endif
#ifdef True
#   undef True
#endif
#ifdef False
#   undef False
#endif


#ifndef PLATFORM_LINUX
#   define PLATFORM_LINUX 1
#endif

#ifndef ENGINE_DLL
#   define ENGINE_DLL 1
#endif

#ifndef GL_SUPPORTED
#   define GL_SUPPORTED 1
#endif

#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"

#if VULKAN_SUPPORTED
#   include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#endif

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

#include "Common/interface/RefCntAutoPtr.h"


using namespace Diligent;

#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#   define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#endif

#ifndef GLX_CONTEXT_MINOR_VERSION_ARB
#   define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
#endif

#ifndef GLX_CONTEXT_FLAGS_ARB
#   define GLX_CONTEXT_FLAGS_ARB               0x2094
#endif

#ifndef GLX_CONTEXT_DEBUG_BIT_ARB
#   define GLX_CONTEXT_DEBUG_BIT_ARB           0x0001
#endif

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, int, const int*);
 
#if VULKAN_SUPPORTED
struct XCBInfo
{
    xcb_connection_t* connection = nullptr;
    uint32_t window = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    xcb_intern_atom_reply_t* atom_wm_delete_window = nullptr;
};
#endif

// For this tutorial, we will use simple vertex shader
// that creates a procedural triangle

// Diligent Engine can use HLSL source for all supported platforms
// It will convert HLSL to GLSL for OpenGL/Vulkan

static const char* VSSource = R"(
struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float3 Color : COLOR; 
};

PSInput main(uint VertId : SV_VertexID) 
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);

    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue

    PSInput ps; 
    ps.Pos = Pos[VertId];
    ps.Color = Col[VertId];
    return ps;
}
)";

// Pixel shader will simply output interpolated vertex color
static const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float3 Color : COLOR; 
};

float4 main(PSInput In) : SV_Target
{
    return float4(In.Color.rgb, 1.0);
}
)";


class Tutorial00App
{
public:
    Tutorial00App()
    {
    }

    bool OnGLContextCreated(Display *display, Window NativeWindowHandle)
    {
        SwapChainDesc SCDesc;
        SCDesc.SamplesCount = 1;
        Uint32 NumDeferredCtx = 0;
        // Declare function pointer
        auto *pFactoryOpenGL = GetEngineFactoryOpenGL();
        EngineGLCreateInfo CreationAttribs;
        CreationAttribs.pNativeWndHandle = reinterpret_cast<void*>(static_cast<size_t>(NativeWindowHandle));
        CreationAttribs.pDisplay = display;
        pFactoryOpenGL->CreateDeviceAndSwapChainGL(
            CreationAttribs, &m_pDevice, &m_pImmediateContext, SCDesc, &m_pSwapChain);

        return true;
    }

#if VULKAN_SUPPORTED
    bool InitVulkan(XCBInfo& xcbInfo)
    {
        EngineVkCreateInfo EngVkAttribs;
#ifdef _DEBUG
        EngVkAttribs.EnableValidation = true;
#endif
        auto* pFactoryVk = GetEngineFactoryVk();
        pFactoryVk->CreateDeviceAndContextsVk(EngVkAttribs, &m_pDevice, &m_pImmediateContext);
        SwapChainDesc SCDesc;
        SCDesc.SamplesCount = 1;
        pFactoryVk->CreateSwapChainVk(m_pDevice, m_pImmediateContext, SCDesc, &xcbInfo, &m_pSwapChain);
    }
#endif

    void CreateResources()
    {
        // Pipeline state object encompasses configuration of all GPU stages

        PipelineStateDesc PSODesc;
        // Pipeline state name is used by the engine to report issues
        // It is always a good idea to give objects descriptive names
        PSODesc.Name = "Simple triangle PSO";

        // This is a graphics pipeline
        PSODesc.IsComputePipeline = false;

        // This tutorial will render to a single render target
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
        // This tutorial will not use depth buffer
        PSODesc.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // No back face culling for this tutorial
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        // Disable depth testing
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        ShaderCreateInfo ShaderCI;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL behind the scene
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.UseCombinedTextureSamplers = true;
        // Create vertex shader
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Triangle vertex shader";
            ShaderCI.Source = VSSource;
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        // Create pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Triangle pixel shader";
            ShaderCI.Source = PSSource;
            m_pDevice->CreateShader(ShaderCI, &pPS);
        }

        // Finally, create the pipeline state
        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;
        m_pDevice->CreatePipelineState(PSODesc, &m_pPSO);
    }

    void Render()
    {
        // Clear the back buffer 
        const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f };
        m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Set pipeline state in the immediate context
        m_pImmediateContext->SetPipelineState(m_pPSO);
        // We need to commit shader resource. Even though in this example
        // we don't really have any resources, this call also sets the shaders
        m_pImmediateContext->CommitShaderResources(nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 3; // We will render 3 vertices
        m_pImmediateContext->Draw(drawAttrs);
    }

    void Present()
    {
        m_pSwapChain->Present();
    }

    void WindowResize(Uint32 Width, Uint32 Height)
    {
        if(m_pSwapChain)
            m_pSwapChain->Resize(Width, Height);
    }

    DeviceType GetDeviceType()const{return m_DeviceType;}
private:
    RefCntAutoPtr<IRenderDevice> m_pDevice;
    RefCntAutoPtr<IDeviceContext> m_pImmediateContext;
    RefCntAutoPtr<ISwapChain> m_pSwapChain;
    RefCntAutoPtr<IPipelineState> m_pPSO;
    DeviceType m_DeviceType = DeviceType::OpenGL;
};

using namespace Diligent;


#if VULKAN_SUPPORTED

XCBInfo InitXCBConnectionAndWindow()
{
    XCBInfo info;

    int scr = 0;
    info.connection = xcb_connect(nullptr, &scr);
    if (info.connection == nullptr || xcb_connection_has_error(info.connection))
    {
        std::cerr << "Unable to make an XCB connection\n";
        exit(-1);
    }

    const xcb_setup_t* setup = xcb_get_setup(info.connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    while (scr-- > 0)
        xcb_screen_next(&iter);

    auto screen = iter.data;

    info.width = 1024;
    info.height = 768;

    uint32_t value_mask, value_list[32];

    info.window = xcb_generate_id(info.connection);

    value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    value_list[0] = screen->black_pixel;
    value_list[1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    xcb_create_window(info.connection, XCB_COPY_FROM_PARENT, info.window, screen->root, 0, 0, info.width, info.height, 0,
                        XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, value_mask, value_list);

    // Magic code that will send notification when window is destroyed
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(info.connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(info.connection, cookie, 0);

    xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(info.connection, 0, 16, "WM_DELETE_WINDOW");
    info.atom_wm_delete_window = xcb_intern_atom_reply(info.connection, cookie2, 0);

    xcb_change_property(info.connection, XCB_PROP_MODE_REPLACE, info.window, (*reply).atom, 4, 32, 1,
                        &(*info.atom_wm_delete_window).atom);
    free(reply);

    const char* title = "Tutorial00: Hello Linux (Vulkan)";
    xcb_change_property(info.connection, XCB_PROP_MODE_REPLACE, info.window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
                        8, strlen(title), title );

    xcb_map_window(info.connection, info.window);

    // Force the x/y coordinates to 100,100 results are identical in consecutive
    // runs
    const uint32_t coords[] = {100, 100};
    xcb_configure_window(info.connection, info.window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, coords);
    xcb_flush(info.connection);

    xcb_generic_event_t *e;
    while ((e = xcb_wait_for_event(info.connection)))
    {
        if ((e->response_type & ~0x80) == XCB_EXPOSE) break;
    }
    return info;
}

void DestroyXCBConnectionAndWindow(XCBInfo &info)
{
    xcb_destroy_window(info.connection, info.window);
    xcb_disconnect(info.connection);
}

int xcb_main()
{
    std::unique_ptr<Tutorial00App> TheApp(new Tutorial00App);

    auto xcbInfo = InitXCBConnectionAndWindow();
    TheApp->InitVulkan(xcbInfo);
    TheApp->CreateResources();
    xcb_flush(xcbInfo.connection);
    while (true)
	{
		xcb_generic_event_t* event;
        bool Quit = false;
		while ((event = xcb_poll_for_event(xcbInfo.connection)) != nullptr)
		{
            switch (event->response_type & 0x7f)
            {
                case XCB_CLIENT_MESSAGE:
                    if ((*(xcb_client_message_event_t*)event).data.data32[0] ==
                        (*xcbInfo.atom_wm_delete_window).atom)
                    {
                        Quit = true;
                    }
                break;

                case XCB_KEY_RELEASE:
                {
                    const auto* keyEvent = reinterpret_cast<const xcb_key_release_event_t*>(event);
                    switch (keyEvent->detail)
                    {
                        #define KEY_ESCAPE 0x9
                        case KEY_ESCAPE:
                            Quit = true;
                            break;
                    }
                }
                break;

                case XCB_DESTROY_NOTIFY:
                    Quit = true;
                break;

                case XCB_CONFIGURE_NOTIFY:
                {
                    const auto* cfgEvent = reinterpret_cast<const xcb_configure_notify_event_t*>(event);
                    if ((cfgEvent->width != xcbInfo.width) || (cfgEvent->height != xcbInfo.height))
                    {
                        xcbInfo.width  = cfgEvent->width;
                        xcbInfo.height = cfgEvent->height;
                        if ((xcbInfo.width > 0) && (xcbInfo.height > 0))
                        {
                            TheApp->WindowResize(xcbInfo.width, xcbInfo.height);
                        }
                    }
                }
                break;
                                    
                default:
                    break;
            }
			free(event);
		}

        if (Quit)
            break;

        TheApp->Render();
        TheApp->Present();
    }
    TheApp.reset();
    DestroyXCBConnectionAndWindow(xcbInfo);
    return 0;
}

#endif

int x_main()
{
    std::unique_ptr<Tutorial00App> TheApp(new Tutorial00App);
    Display *display = XOpenDisplay(0);

    static int visual_attribs[] =
    {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER,   true,

        // The largest available total RGBA color buffer size (sum of GLX_RED_SIZE, 
        // GLX_GREEN_SIZE, GLX_BLUE_SIZE, and GLX_ALPHA_SIZE) of at least the minimum
        // size specified for each color component is preferred.
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_ALPHA_SIZE,     8,

        // The largest available depth buffer of at least GLX_DEPTH_SIZE size is preferred
        GLX_DEPTH_SIZE,     24,

        //GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, 1,
        None
     };
 
    int fbcount = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
    if (!fbc)
    {
        std::cerr << "Failed to retrieve a framebuffer config\n";
        return -1;
    }
 
    XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[0]);
 
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = 
        StructureNotifyMask |  
        ExposureMask |  
        KeyPressMask | 
        KeyReleaseMask |
        ButtonPressMask | 
        ButtonReleaseMask | 
        PointerMotionMask;
 
    Window win = XCreateWindow(display, RootWindow(display, vi->screen), 0, 0, 1024, 768, 0, vi->depth, InputOutput, vi->visual, CWBorderPixel|CWColormap|CWEventMask, &swa);
    if (!win)
    {
        std::cerr <<  "Failed to create window.\n";
        return -1;
    }
 
    XMapWindow(display, win);
 
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = nullptr;
    {
        // Create an oldstyle context first, to get the correct function pointer for glXCreateContextAttribsARB
        GLXContext ctx_old = glXCreateContext(display, vi, 0, GL_TRUE);
        glXCreateContextAttribsARB =  (glXCreateContextAttribsARBProc)glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, ctx_old);
    }
 
    if (glXCreateContextAttribsARB == nullptr)
    {
        std::cerr << "glXCreateContextAttribsARB entry point not found. Aborting.\n";
        return -1;
    }
 
    int Flags = GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
#ifdef _DEBUG
     Flags |= GLX_CONTEXT_DEBUG_BIT_ARB;
#endif 
    
    int major_version = 4;
    int minor_version = 3;
    static int context_attribs[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, major_version,
        GLX_CONTEXT_MINOR_VERSION_ARB, minor_version,
        GLX_CONTEXT_FLAGS_ARB, Flags,
        None
    };
  
    constexpr int True = 1;
    GLXContext ctx = glXCreateContextAttribsARB(display, fbc[0], NULL, True, context_attribs);
    if (!ctx)
    {
        std::cerr << "Failed to create GL context.\n";
        return -1;
    }
    XFree(fbc);
    

    glXMakeCurrent(display, win, ctx);
    TheApp->OnGLContextCreated(display, win);
    TheApp->CreateResources();
    XStoreName(display, win, "Tutorial00: Hello Linux (OpenGL)");
    while (true) 
    {
        bool EscPressed = false;
        XEvent xev;
        // Handle all events in the queue
        while(XCheckMaskEvent(display, 0xFFFFFFFF, &xev))
        {
            switch(xev.type)
            {
                case KeyPress:
                {
                    KeySym keysym;
                    char buffer[80];
                    int num_char = XLookupString((XKeyEvent *)&xev, buffer, _countof(buffer), &keysym, 0);
                    EscPressed = (keysym==XK_Escape);
                }

                case ConfigureNotify:
                {
                    XConfigureEvent &xce = reinterpret_cast<XConfigureEvent &>(xev);
                    if(xce.width != 0 && xce.height != 0)
                        TheApp->WindowResize(xce.width, xce.height);
                    break;
                }
            }
        }

        if(EscPressed)
            break;

        TheApp->Render();

        TheApp->Present();
    }

    TheApp.reset();
 
    ctx = glXGetCurrentContext();
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, ctx);
    XDestroyWindow(display, win);
    XCloseDisplay(display);

    return 0;
}

int main (int argc, char ** argv)
{
    DeviceType DevType = DeviceType::OpenGL;

#ifdef VULKAN_SUPPORTED
    DevType = DeviceType::Vulkan;
    if (argc > 1)
    {
        const auto* Key = "-mode ";
        const auto* pos = strstr(argv[1], Key);
        if (pos != nullptr)
        {
            pos += strlen(Key);
            if (strcasecmp(pos, "GL") == 0)
            {
                DevType = DeviceType::OpenGL;
            }
            else if (strcasecmp(pos, "VK") == 0)
            {
                DevType = DeviceType::Vulkan;
            }
            else
            {
                std::cerr << "Unknown device type. Only the following types are supported: GL, VK";
                return -1;
            }
        }
    }

    if (DevType == DeviceType::Vulkan)
    {
        return xcb_main();
    }
#endif

    return x_main();
}
