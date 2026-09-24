#include "composition_renderer.hpp"

#include "../core/math.hpp"

#include <algorithm>
#include <cmath>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dcomp.lib")

namespace pets {

void Renderer::initialize(HWND hwnd) {
    window = hwnd;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL level{};

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, nullptr, 0, D3D11_SDK_VERSION,
        &device, &level, &immediate);

    if (FAILED(hr)) {
        check(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            flags, nullptr, 0, D3D11_SDK_VERSION,
            &device, &level, &immediate));
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    check(device.As(&dxgiDevice));

    D2D1_FACTORY_OPTIONS options{};

    check(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(factory.GetAddressOf())));

    check(factory->CreateDevice(dxgiDevice.Get(), &d2dDevice));
    check(d2dDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context));

    context->SetDpi(96, 96);
    context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    check(context->CreateSolidColorBrush(color(0xFFFFFF), &solid));

    D2D1_STROKE_STYLE_PROPERTIES strokeProperties =
        D2D1::StrokeStyleProperties();

    strokeProperties.startCap = D2D1_CAP_STYLE_ROUND;
    strokeProperties.endCap = D2D1_CAP_STYLE_ROUND;
    strokeProperties.lineJoin = D2D1_LINE_JOIN_ROUND;

    check(factory->CreateStrokeStyle(
        strokeProperties, nullptr, 0, &stroke));

    ComPtr<IDXGIAdapter> adapter;
    check(dxgiDevice->GetAdapter(&adapter));

    ComPtr<IDXGIFactory2> dxgiFactory;
    check(adapter->GetParent(
        IID_PPV_ARGS(dxgiFactory.GetAddressOf())));

    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = 64;
    description.Height = 64;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    check(dxgiFactory->CreateSwapChainForComposition(
        device.Get(), &description, nullptr, &swap));

    check(DCompositionCreateDevice(
        dxgiDevice.Get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(composition.GetAddressOf())));

    check(composition->CreateTargetForHwnd(window, TRUE, &target));
    check(composition->CreateVisual(&visual));
    check(visual->SetContent(swap.Get()));
    check(target->SetRoot(visual.Get()));
    check(composition->Commit());

    width = height = 64;
    makeTarget();
}

void Renderer::draw(const Scene& scene, bool picking, const RECT& desktop) {
    Box box = scene.box;

    if (!box.valid) return;

    int left, top, right, bottom;

    if (picking) {
        left = desktop.left;
        top = desktop.top;
        right = desktop.right;
        bottom = desktop.bottom;
    } else {
        constexpr double tile = 32;

        left = static_cast<int>(std::floor((box.l - 5) / tile) * tile);
        top = static_cast<int>(std::floor((box.t - 5) / tile) * tile);
        right = static_cast<int>(std::ceil((box.r + 5) / tile) * tile);
        bottom = static_cast<int>(std::ceil((box.b + 5) / tile) * tile);
    }

    resize(std::max(32, right - left), std::max(32, bottom - top));

    SetWindowPos(
        window, HWND_TOPMOST, left, top, width, height,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    if (picking) {
        SetWindowRgn(window, nullptr, FALSE);
    } else {
        HRGN region = scene.region(left, top);
        if (!SetWindowRgn(window, region, FALSE))
            DeleteObject(region);
    }

    context->BeginDraw();
    context->SetTransform(D2D1::Matrix3x2F::Identity());
    context->Clear(color(0, 0));

    context->SetTransform(
        D2D1::Matrix3x2F::Translation(
            static_cast<float>(-left),
            static_cast<float>(-top)));

    for (const Draw& command : scene.draws)
        drawCommand(command);

    check(context->EndDraw());
    check(swap->Present(1, 0));
}

D2D1_POINT_2F Renderer::point(V p) {
    return {static_cast<float>(p.x), static_cast<float>(p.y)};
}

void Renderer::makeTarget() {
    ComPtr<IDXGISurface> surface;
    check(swap->GetBuffer(0, IID_PPV_ARGS(surface.GetAddressOf())));

    D2D1_BITMAP_PROPERTIES1 properties =
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET |
                D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED),
            96, 96);

    check(context->CreateBitmapFromDxgiSurface(
        surface.Get(), &properties, &bitmap));

    context->SetTarget(bitmap.Get());
}

void Renderer::resize(int w, int h) {
    if (w == width && h == height) return;

    context->SetTarget(nullptr);
    bitmap.Reset();

    check(swap->ResizeBuffers(
        2, static_cast<UINT>(w), static_cast<UINT>(h),
        DXGI_FORMAT_B8G8R8A8_UNORM, 0));

    width = w;
    height = h;
    makeTarget();
}

void Renderer::drawCommand(const Draw& d) {
    solid->SetColor(d.a);
    ID2D1Brush* brush = solid.Get();

    ComPtr<ID2D1LinearGradientBrush> gradient;
    ComPtr<ID2D1GradientStopCollection> stops;

    if (d.gradient) {
        D2D1_GRADIENT_STOP data[2]{
            {0, d.a},
            {1, d.b}
        };

        check(context->CreateGradientStopCollection(
            data, 2, &stops));

        Box bounds;

        if (d.shape == Shape::Ellipse) {
            bounds.add(d.points[0] - d.radius);
            bounds.add(d.points[0] + d.radius);
        } else {
            for (V p : d.points) bounds.add(p);
        }

        auto properties = D2D1::LinearGradientBrushProperties(
            point({bounds.l, bounds.t}),
            point({bounds.r, bounds.b}));

        check(context->CreateLinearGradientBrush(
            properties, stops.Get(), &gradient));

        brush = gradient.Get();
    }

    if (d.shape == Shape::Ellipse) {
        context->FillEllipse(
            D2D1::Ellipse(
                point(d.points[0]),
                static_cast<float>(d.radius.x),
                static_cast<float>(d.radius.y)),
            brush);
        return;
    }

    if (d.shape == Shape::Line) {
        context->DrawLine(
            point(d.points[0]), point(d.points[1]),
            brush, static_cast<float>(d.width), stroke.Get());
        return;
    }

    ComPtr<ID2D1PathGeometry> geometry;
    ComPtr<ID2D1GeometrySink> sink;

    check(factory->CreatePathGeometry(&geometry));
    check(geometry->Open(&sink));

    sink->BeginFigure(point(d.points.front()), D2D1_FIGURE_BEGIN_FILLED);

    for (size_t i = 1; i < d.points.size(); ++i)
        sink->AddLine(point(d.points[i]));

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    check(sink->Close());

    context->FillGeometry(geometry.Get(), brush);
}

} // namespace pets
