cmake_minimum_required (VERSION 3.6)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(DiligentEngine)

option(DILIGENT_BUILD_FX "Build DiligentFX module" ON)
option(DILIGENT_BUILD_SAMPLES "Build DiligentSamples module" ON)
option(DILIGENT_BUILD_DEMOS "Build demo projects" ON)
option(DILIGENT_BUILD_UNITY_PLUGIN "Build Unity plugin" ON)


#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules" ${CMAKE_MODULE_PATH})

# glm
#set(GLM_TEST_ENABLE OFF)
#set(BUILD_SHARED_LIBS OFF)

# assimp
#set(ASSIMP_BUILD_ZLIB OFF)

add_subdirectory(thirdparty/glm-0.9.9.6)
add_subdirectory(thirdparty/assimp-5.0.0)
add_subdirectory(thirdparty/fibertaskinglib)

#add_subdirectory(thirdparty/zlib)
add_subdirectory(thirdparty/freeimage)

add_subdirectory(DiligentCore)
add_subdirectory(DiligentTools)
add_subdirectory(Common)

if(${DILIGENT_BUILD_FX})
    add_subdirectory(DiligentFX)
endif()

if(${DILIGENT_BUILD_SAMPLES})
    add_subdirectory(DiligentSamples)
endif()

if(${DILIGENT_BUILD_DEMOS})
    add_subdirectory(Projects)
endif()

if(${DILIGENT_BUILD_UNITY_PLUGIN} AND (D3D11_SUPPORTED OR D3D12_SUPPORTED OR GL_SUPPORTED OR GLES_SUPPORTED))
    add_subdirectory(unityplugin)
endif()

if(DILIGENT_ENABLE_TESTS)
    add_subdirectory(Tests)
endif()

if(TARGET GLTFViewer)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT GLTFViewer)
elseif(TARGET Asteroids)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Asteroids)
elseif(TARGET AtmosphereSample)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT AtmosphereSample)
endif()
