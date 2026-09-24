#pragma once

#include "../core/math.hpp"

#include <vector>

namespace pets {

struct TwoBone {
    V joint;
    V end;
};

inline TwoBone ik2(
    V root, V target,
    double a, double b, double bend)
{
    V axis = unit(target - root);

    double d = std::clamp(
        dist(root, target),
        std::max(1e-6, std::abs(a - b)),
        a + b
    );

    double x = (a * a - b * b + d * d) / (2 * d);
    double y = std::sqrt(std::max(0.0, a * a - x * x));

    return {
        root + axis * x + perp(axis) * (y * bend),
        root + axis * d
    };
}

inline void fabrik(
    std::vector<V>& p,
    V root, V target,
    double length,
    int passes = 28)
{
    double reach = length * static_cast<double>(p.size() - 1);

    if (dist(root, target) >= reach) {
        V axis = unit(target - root);
        p[0] = root;

        for (size_t i = 1; i < p.size(); ++i)
            p[i] = p[i - 1] + axis * length;

        return;
    }

    for (int pass = 0; pass < passes; ++pass) {
        p.back() = target;

        for (size_t i = p.size() - 1; i > 0; --i)
            p[i - 1] = p[i] + unit(p[i - 1] - p[i]) * length;

        p[0] = root;

        for (size_t i = 1; i < p.size(); ++i)
            p[i] = p[i - 1] + unit(p[i] - p[i - 1]) * length;

        if (dist(p.back(), target) < 0.04)
            break;
    }
}

// Verlet 触丝：位置约束决定骨长，速度来自历史位置。
class Filament {
public:
    std::vector<V> p;
    std::vector<V> previous;
    double segment = 8;

    Filament(int count = 20, double spacing = 8)
        : p(static_cast<size_t>(count)),
          previous(static_cast<size_t>(count)),
          segment(spacing)
    {
    }

    void reset(V anchor, V axis) {
        axis = unit(axis);

        for (size_t i = 0; i < p.size(); ++i)
            p[i] = anchor + axis * (segment * static_cast<double>(i));

        previous = p;
    }

    void translate(V delta) {
        for (V& q : p) q += delta;
        for (V& q : previous) q += delta;
    }

    void tick(
        double dt, V anchor, V force,
        bool pinTip = false, V tip = {})
    {
        for (size_t i = 1; i < p.size(); ++i) {
            V old = p[i];
            p[i] += (p[i] - previous[i]) * 0.987 + force * (dt * dt);
            previous[i] = old;
        }

        for (int pass = 0; pass < 20; ++pass) {
            p.front() = anchor;
            if (pinTip) p.back() = tip;

            for (size_t i = 0; i + 1 < p.size(); ++i) {
                V d = p[i + 1] - p[i];
                V correction = unit(d) * (len(d) - segment);

                bool fixedA = i == 0;
                bool fixedB = pinTip && i + 1 == p.size() - 1;

                if (fixedA && fixedB) continue;

                if (fixedA) p[i + 1] -= correction;
                else if (fixedB) p[i] += correction;
                else {
                    p[i] += correction * 0.5;
                    p[i + 1] -= correction * 0.5;
                }
            }
        }

        p.front() = anchor;
        if (pinTip) p.back() = tip;
    }
};

} // namespace pets
