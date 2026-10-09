#pragma once

// Scene 使用世界坐标。渲染器负责裁剪与坐标变换。
// 图形轮廓同时用于构造窗口区域，透明空隙不占鼠标。

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
#include <d2d1_1.h>

#include "../core/math.hpp"

#include <array>
#include <utility>
#include <vector>

#pragma comment(lib, "gdi32.lib")

namespace pets {

using Color = D2D1_COLOR_F;

inline Color color(unsigned hex, float alpha = 1) {
    return {
        static_cast<float>((hex >> 16) & 255) / 255,
        static_cast<float>((hex >> 8) & 255) / 255,
        static_cast<float>(hex & 255) / 255,
        alpha
    };
}

enum class Shape {
    Ellipse,
    Polygon,
    Line
};

struct Draw {
    Shape shape = Shape::Line;
    std::vector<V> points;
    V radius{};
    double width = 1;

    Color a{};
    Color b{};

    bool gradient = false;
    bool hitTestable = true;
};

class Scene {
public:
    std::vector<Draw> draws;
    Box box;

    void ellipse(V center, V radius, Color c) {
        Draw d;
        d.shape = Shape::Ellipse;
        d.points = {center};
        d.radius = radius;
        d.a = c;

        draws.push_back(std::move(d));

        box.add({center.x - radius.x, center.y - radius.y}, 3);
        box.add({center.x + radius.x, center.y + radius.y}, 3);
    }

    void ellipse(V center, V radius, Color a, Color b) {
        ellipse(center, radius, a);
        draws.back().gradient = true;
        draws.back().b = b;
    }

    void disk(V center, double radius, Color c) {
        ellipse(center, {radius, radius}, c);
    }

    void line(V a, V b, Color c, double width = 2) {
        Draw d;
        d.shape = Shape::Line;
        d.points = {a, b};
        d.width = width;
        d.a = c;

        draws.push_back(std::move(d));
        box.add(a, width + 3);
        box.add(b, width + 3);
    }

    void poly(std::vector<V> points, Color c) {
        Draw d;
        d.shape = Shape::Polygon;
        d.points = std::move(points);
        d.a = c;

        for (V p : d.points) box.add(p, 3);
        draws.push_back(std::move(d));
    }

    void poly(std::vector<V> points, Color a, Color b) {
        poly(std::move(points), a);
        draws.back().gradient = true;
        draws.back().b = b;
    }

    void chain(const std::vector<V>& p, Color c, double width) {
        for (size_t i = 1; i < p.size(); ++i)
            line(p[i - 1], p[i], c, width);
    }

    void ring(V center, double rx, double ry, Color c, double width) {
        V last = center + V{rx, 0};

        for (int i = 1; i <= 48; ++i) {
            double a = TAU * i / 48;
            V next = center + V{std::cos(a) * rx, std::sin(a) * ry};
            line(last, next, c, width);
            last = next;
        }
    }

    HRGN region(int originX, int originY) const {
        HRGN result = CreateRectRgn(0, 0, 0, 0);

        auto merge = [&](HRGN part) {
            CombineRgn(result, result, part, RGN_OR);
            DeleteObject(part);
        };

        auto circleRegion = [&](V p, double radius) {
            merge(CreateEllipticRgn(
                static_cast<int>(std::floor(p.x - radius)) - originX,
                static_cast<int>(std::floor(p.y - radius)) - originY,
                static_cast<int>(std::ceil(p.x + radius)) - originX,
                static_cast<int>(std::ceil(p.y + radius)) - originY));
        };

        for (const Draw& d : draws) {
            if (d.a.a < 0.015f) continue;
            if (!d.hitTestable) continue;

            if (d.shape == Shape::Ellipse) {
                V p = d.points[0];

                merge(CreateEllipticRgn(
                    static_cast<int>(std::floor(p.x - d.radius.x - 2)) - originX,
                    static_cast<int>(std::floor(p.y - d.radius.y - 2)) - originY,
                    static_cast<int>(std::ceil(p.x + d.radius.x + 2)) - originX,
                    static_cast<int>(std::ceil(p.y + d.radius.y + 2)) - originY));
            } else if (d.shape == Shape::Polygon) {
                std::vector<POINT> points;

                for (V p : d.points) {
                    points.push_back({
                        static_cast<LONG>(std::lround(p.x)) - originX,
                        static_cast<LONG>(std::lround(p.y)) - originY
                    });
                }

                merge(CreatePolygonRgn(
                    points.data(), static_cast<int>(points.size()), WINDING));
            } else {
                V a = d.points[0], b = d.points[1];
                double radius = std::max(2.0, d.width * 0.5 + 1.5);
                V side = perp(unit(b - a)) * radius;

                std::array<V, 4> vertices{
                    a + side, b + side, b - side, a - side
                };

                POINT points[4]{};

                for (int i = 0; i < 4; ++i) {
                    points[i] = {
                        static_cast<LONG>(std::lround(vertices[i].x)) - originX,
                        static_cast<LONG>(std::lround(vertices[i].y)) - originY
                    };
                }

                merge(CreatePolygonRgn(points, 4, WINDING));
                circleRegion(a, radius);
                circleRegion(b, radius);
            }
        }

        return result;
    }
};

} // namespace pets
