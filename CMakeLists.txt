cmake_minimum_required(VERSION 3.24)

project(DesktopPetsGen3 LANGUAGES CXX)

add_executable(DesktopPetsGen3 WIN32
    main.cpp
    application.cpp
    render/composition_renderer.cpp
    desktop/environment.cpp
    platform/settings.cpp
    audio/procedural_audio.cpp
    characters/spider.cpp
    characters/youxi.cpp
    characters/plant.cpp
)

target_compile_features(DesktopPetsGen3 PRIVATE cxx_std_20)

target_compile_definitions(DesktopPetsGen3 PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    NTDDI_VERSION=0x0A000000
)

target_include_directories(DesktopPetsGen3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(DesktopPetsGen3 PRIVATE
    user32
    gdi32
    ole32
    shell32
    dwmapi
    winmm
    d3d11
    dxgi
    d2d1
    dcomp
)

if(MSVC)
    target_compile_options(DesktopPetsGen3 PRIVATE
        /W4
        /utf-8
        /permissive-
    )
else()
    target_compile_options(DesktopPetsGen3 PRIVATE
        -Wall
        -Wextra
        -municode
    )

    target_link_options(DesktopPetsGen3 PRIVATE
        -municode
        -static-libgcc
        -static-libstdc++
        -static
    )
endif()
