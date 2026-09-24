#pragma once

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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>
#include <stdexcept>

namespace pets {

constexpr double PI = std::numbers::pi;
constexpr double TAU = PI * 2.0;
constexpr double STEP = 1.0 / 120.0;

struct V {
    double x = 0;
    double y = 0;

    V operator+(V b) const { return {x + b.x, y + b.y}; }
    V operator-(V b) const { return {x - b.x, y - b.y}; }
    V operator-() const { return {-x, -y}; }
    V operator*(double s) const { return {x * s, y * s}; }
    V operator/(double s) const { return {x / s, y / s}; }

    V& operator+=(V b) { x += b.x; y += b.y; return *this; }
    V& operator-=(V b) { x -= b.x; y -= b.y; return *this; }
    V& operator*=(double s) { x *= s; y *= s; return *this; }
};

inline V operator*(double s, V v) { return v * s; }
inline double dot(V a, V b) { return a.x * b.x + a.y * b.y; }
inline double cross(V a, V b) { return a.x * b.y - a.y * b.x; }
inline double len(V v) { return std::hypot(v.x, v.y); }
inline double dist(V a, V b) { return len(a - b); }
inline V perp(V v) { return {-v.y, v.x}; }

inline V unit(V v, V fallback = {1, 0}) {
    double n = len(v);
    return n > 1e-8 ? v / n : fallback;
}

inline V polar(double a) {
    return {std::cos(a), std::sin(a)};
}

inline V rotate(V v, double a) {
    double c = std::cos(a), s = std::sin(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

inline V mix(V a, V b, double t) {
    return a + (b - a) * t;
}

inline double smooth(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

inline double follow(double speed, double dt) {
    return 1.0 - std::exp(-speed * dt);
}

inline double wrapAngle(double a) {
    return std::remainder(a, TAU);
}

inline V limit(V v, double maximum) {
    double n = len(v);
    return n > maximum ? v * (maximum / n) : v;
}

inline V closestOnSegment(V p, V a, V b) {
    V d = b - a;
    double q = dot(d, d);
    double t = q > 1e-9 ? dot(p - a, d) / q : 0;
    return a + d * std::clamp(t, 0.0, 1.0);
}

inline double segmentDistance(V p, V a, V b) {
    return dist(p, closestOnSegment(p, a, b));
}

inline void spring(
    V& p, V& velocity, V target,
    double stiffness, double damping, double dt)
{
    velocity += ((target - p) * stiffness - velocity * damping) * dt;
    p += velocity * dt;
}

struct Box {
    double l = 0, t = 0, r = 0, b = 0;
    bool valid = false;

    void add(V p, double radius = 0) {
        if (!valid) {
            l = p.x - radius;
            r = p.x + radius;
            t = p.y - radius;
            b = p.y + radius;
            valid = true;
            return;
        }

        l = std::min(l, p.x - radius);
        r = std::max(r, p.x + radius);
        t = std::min(t, p.y - radius);
        b = std::max(b, p.y + radius);
    }

    bool contains(V p) const {
        return p.x >= l && p.x <= r &&
               p.y >= t && p.y <= b;
    }
};

inline void check(HRESULT hr) {
    if (FAILED(hr)) {
        char text[80]{};
        sprintf_s(text, "Windows graphics error: 0x%08lX",
                  static_cast<unsigned long>(hr));
        throw std::runtime_error(text);
    }
}

} // namespace pets
