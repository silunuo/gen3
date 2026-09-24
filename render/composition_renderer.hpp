#pragma once

// D3D11 + DXGI composition swap chain
// D2D1 device context
// DirectComposition visual
//
// 真正的预乘 Alpha，不使用黑色抠图或 GDI 颜色转 Alpha。

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "scene.hpp"

namespace pets {

using Microsoft::WRL::ComPtr;

class Renderer {
public:
    void initialize(HWND hwnd);
    void draw(const Scene& scene, bool picking, const RECT& desktop);

private:
    HWND window{};
    int width = 0, height = 0;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> immediate;

    ComPtr<ID2D1Factory1> factory;
    ComPtr<ID2D1Device> d2dDevice;
    ComPtr<ID2D1DeviceContext> context;
    ComPtr<ID2D1Bitmap1> bitmap;

    ComPtr<ID2D1SolidColorBrush> solid;
    ComPtr<ID2D1StrokeStyle> stroke;

    ComPtr<IDXGISwapChain1> swap;

    ComPtr<IDCompositionDevice> composition;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual> visual;

    static D2D1_POINT_2F point(V p);

    void makeTarget();
    void resize(int w, int h);
    void drawCommand(const Draw& d);
};

} // namespace pets
