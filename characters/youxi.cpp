#include "youxi.hpp"

#include <cmath>

namespace pets {

void Youxi::initialize(World& w, const Settings& settings) {
    center = settings.position(
        L"Youxi", w.desktop.home);

    if (!w.desktop.onScreen(center))
        center = w.desktop.home;

    nest = {
        settings.get(L"Youxi", L"nestX", center.x + 130),
        settings.get(L"Youxi", L"nestY", center.y + 90)
    };

    if (!w.desktop.onScreen(nest))
        nest = center + V{130, 90};

    age = settings.get(L"Youxi", L"age", 0);
    weave = settings.get(L"Youxi", L"weave", 1);
    target = center;

    for (int i = 0; i < membraneCount; ++i) {
        double a = membraneAngle(i);

        membrane[i] = center + V{
            std::cos(a) * 77 * w.scale,
            std::sin(a) * 59 * w.scale
        };
    }

    nodes[0] = center + V{90, -28} * w.scale;
    nodes[1] = center + V{-74, -24} * w.scale;
    nodes[2] = center + V{8, 65} * w.scale;

    longThread.segment = 8 * w.scale;
    shortThread.segment = 6 * w.scale;

    longThread.reset(membrane.front(), {0.4, 1});
    shortThread.reset(membrane.back(), {0.8, 0.6});
}

bool Youxi::down(V p, World& w) {
    double s = w.scale;

    for (Echo& echo : echoes) {
        if (dist(p, echo.p) < echo.radius * s + 12 * s) {
            echo.velocity += unit(p - center) * (95 * s);
            echo.life = std::max(echo.life, 1.2);
            w.audio.play(1, 660, 0.08);
            return true;
        }
    }

    if (dist(p, longThread.p.back()) < 20 * s) {
        drag = Drag::Thread;
        offset = longThread.p.back() - p;
        resting = false;
        returning = false;
        w.audio.play(1, 380, 0.07);
        return true;
    }

    if (dist(p, center) < 94 * s) {
        drag = Drag::Body;
        offset = center - p;
        resting = false;
        returning = false;
        return true;
    }

    if (weave > 0.05 && dist(p, nest) < 75 * s) {
        if (dist(p, nest) < 19 * s) {
            drag = Drag::Nest;
            offset = nest - p;
        } else {
            nestWave = 1;
            resting = false;
            target = nest + V{0, -35 * s};
            returning = true;
            w.audio.play(1, 420 + std::abs(p.x - nest.x) * 3, 0.12);
        }

        return true;
    }

    return false;
}

void Youxi::up(World& w) {
    if (drag == Drag::Body) {
        releaseBloom = 1;
        velocity = limit(velocity, 650 * w.scale);
        target = center + velocity * 0.45;
        targetClock = 1.5;

        w.audio.play(1, 520, 0.08);
    }

    if (drag == Drag::Thread) {
        releaseBloom = 1;
        target = center + velocity * 0.5;
        targetClock = 2;

        w.audio.play(1, 720, 0.09);
    }

    if (drag == Drag::Nest) {
        returning = true;
        target = nest + V{0, -25 * w.scale};
        nestWave = 1;
    }

    drag = Drag::None;
}

void Youxi::command(int command, V p, World& w) {
    if (command == Summon) {
        target = p;
        targetClock = 5;
        resting = false;
        returning = false;
    } else if (command == ReturnHome) {
        target = nest + V{0, -20 * w.scale};
        returning = true;
        resting = false;
    } else if (command == PlaceHome) {
        nest = p;
        weave = 0;
        nestWave = 1;
        returning = true;
        resting = false;
        target = nest + V{0, -25 * w.scale};
    }
}

void Youxi::save(const Settings& settings) const {
    settings.setPosition(L"Youxi", center);
    settings.set(L"Youxi", L"nestX", nest.x);
    settings.set(L"Youxi", L"nestY", nest.y);
    settings.set(L"Youxi", L"age", age);
    settings.set(L"Youxi", L"weave", weave);
}

void Youxi::tick(World& w, double dt) {
    age += dt;

    double s = w.scale;

    nestWave *= std::exp(-2.1 * dt);
    releaseBloom *= std::exp(-2.5 * dt);
    throughPulse *= std::exp(-3.4 * dt);

    longThread.segment = 8 * s;
    shortThread.segment = 6 * s;

    updateDisturbance(w, dt);
    updateEchoes(w, dt);

    if (drag == Drag::Nest) {
        nest = mix(nest, w.mouse + offset, follow(22, dt));
        nestWave = 0.65;
    }

    targetClock -= dt;

    if (drag == Drag::Body) {
        V old = center;
        V desired = w.mouse + offset;

        center = mix(center, desired, follow(24, dt));
        velocity = (center - old) / dt;

        resting = false;
    } else if (drag == Drag::Thread) {
        V pin = w.mouse + offset;

        V orbit = pin + V{
            std::sin(age * 1.3) * 102 * s,
            -102 * s + std::cos(age * 1.1) * 29 * s
        };

        spring(center, velocity, orbit, 24, 9, dt);

        V separation = center - pin;
        double maximum = 132 * s;

        if (len(separation) > maximum) {
            center = pin + unit(separation) * maximum;

            V radial = unit(separation);
            double outwardVelocity = dot(velocity, radial);

            if (outwardVelocity > 0)
                velocity -= radial * outwardVelocity;
        }
    } else {
        if (targetClock <= 0 && !returning && !resting) {
            chooseTarget(w);
            targetClock = 4.5 + std::sin(age * 0.4) * 1.2;
        }

        if (returning) {
            target = nest + V{0, -22 * s};

            if (dist(center, target) < 20 * s) {
                weave = std::min(1.0, weave + dt * 0.34);

                if (weave >= 1) {
                    resting = true;
                    returning = false;
                    sleepAge = 0;
                }
            }
        }

        if (resting) {
            sleepAge += dt;
            target = nest + V{
                std::sin(age * 0.6) * 4 * s,
                -17 * s
            };

            if (sleepAge > 12) {
                resting = false;
                targetClock = 0;
                releaseBloom = 0.8;
            }
        }

        double pulseDrive = 0.8 + 0.2 * std::sin(age * 3.3);

        spring(
            center,
            velocity,
            target,
            resting ? 13 : 5.5 * pulseDrive,
            resting ? 8 : 4.8,
            dt);

        velocity += disturbance * dt;
    }

    fold += (
        (resting ? 1.0 : 0.0) - fold
    ) * follow(2.4, dt);

    double speed = len(velocity);
    V desiredFacing = unit(velocity, facing);

    facing = unit(mix(
        facing, desiredFacing, follow(2.9, dt)));

    double rotation = std::atan2(facing.y, facing.x) * 0.22;
    double stretch = std::clamp(speed / (450 * s), 0.0, 0.32);

    V attention = unit(w.mouse - center);
    double mouseNear =
        std::clamp(1.0 - dist(w.mouse, center) / (230 * s), 0.0, 1.0);

    std::array<V, 3> nodeTargets{
        center + rotate(
            V{91, -27 + 13 * std::sin(age * 1.6)} * s,
            rotation),

        center + rotate(
            V{-75, -26 + 9 * std::cos(age * 1.1)} * s,
            rotation),

        center + rotate(
            V{12 * std::sin(age * 0.8), 66} * s,
            rotation)
    };

    nodeTargets[0] += attention * (mouseNear * 14 * s);

    if (fold > 0) {
        for (V& p : nodeTargets)
            p = mix(p, center + (p - center) * 0.4, fold);
    }

    if (weave < 1 && returning) {
        nodeTargets[2] = mix(
            nodeTargets[2],
            nest + polar(age * 2) * (45 * s),
            0.65);
    }

    // 周期性梳膜：织构节点穿过中央开口。
    double groomPhase = std::fmod(age, 27.0);

    if (groomPhase > 21 && groomPhase < 24 &&
        drag == Drag::None && !resting)
    {
        double q = (groomPhase - 21) / 3;

        nodeTargets[2] = center + V{
            std::sin(q * TAU) * 38 * s,
            std::cos(q * TAU) * 20 * s
        };
    }

    for (int i = 0; i < 3; ++i) {
        spring(
            nodes[i], nodeVelocity[i], nodeTargets[i],
            42 + i * 12, 12, dt);
    }

    double pulse = std::sin(age * 3.3);
    double opening =
        1 + throughPulse * 0.18 + releaseBloom * 0.17;

    std::array<V, membraneCount> goals{};
    std::array<V, membraneCount> forces{};

    for (int i = 0; i < membraneCount; ++i) {
        double a = membraneAngle(i);
        double asymmetry = 1 + std::sin(a * 2) * 0.1;

        V local{
            std::cos(a) * (77 + pulse * 6) *
                (1 + stretch) * opening,

            std::sin(a) * (58 - pulse * 4) *
                asymmetry * (1 - stretch * 0.35)
        };

        local.x *= 1 - fold * 0.48;
        local.y *= 1 - fold * 0.34;

        if (fold > 0)
            local.y += std::max(0.0, std::sin(a)) * fold * 12;

        goals[i] = center + rotate(local * s, rotation);

        for (int node = 0; node < 3; ++node) {
            double d = dist(goals[i], nodeTargets[node]) / s;
            double influence = std::exp(-d * d / 4800);

            goals[i] +=
                (nodes[node] - nodeTargets[node]) *
                (influence * 0.55);
        }
    }

    for (int i = 0; i < membraneCount; ++i) {
        forces[i] =
            (goals[i] - membrane[i]) * 85 -
            membraneVelocity[i] * 16;

        if (i > 0) {
            forces[i] += (
                (membrane[i - 1] - membrane[i]) -
                (goals[i - 1] - goals[i])
            ) * 34;
        }

        if (i + 1 < membraneCount) {
            forces[i] += (
                (membrane[i + 1] - membrane[i]) -
                (goals[i + 1] - goals[i])
            ) * 34;
        }
    }

    for (int i = 0; i < membraneCount; ++i) {
        membraneVelocity[i] += forces[i] * dt;
        membrane[i] += membraneVelocity[i] * dt;
    }

    longThread.tick(
        dt,
        membrane.front(),
        {std::sin(age * 1.3) * 33 * s, 57 * s},
        drag == Drag::Thread,
        w.mouse + offset);

    bool touchNest = resting || (returning && dist(center, nest) < 85 * s);

    shortThread.tick(
        dt,
        membrane.back(),
        {std::cos(age * 1.5) * 22 * s, 32 * s},
        touchNest,
        nest + V{31 * s, -3 * s});

    bool cursorInside =
        std::pow((w.mouse.x - center.x) / (47 * s), 2) +
        std::pow((w.mouse.y - center.y) / (33 * s), 2) < 1;

    if (cursorInside && !wasInside &&
        drag == Drag::None && !resting)
    {
        throughPulse = 1;
        nestWave = std::max(nestWave, 0.18);

        w.audio.play(1, 560 + std::sin(age) * 130, 0.09);

        if (echoes.size() < 3) {
            echoes.push_back({
                center,
                velocity * 0.2 + V{0, -15 * s},
                34,
                2.4
            });
        }
    }

    wasInside = cursorInside;

    echoClock -= dt;

    if (echoClock <= 0 && !resting && drag == Drag::None) {
        echoClock = 13 + std::sin(age) * 3;

        echoes.push_back({
            center + facing * (62 * s),
            facing * (23 * s),
            28,
            5
        });
    }

    trailClock -= dt;

    if (speed > 80 * s && trailClock <= 0) {
        trailClock = 0.07;
        trail.push_back({center, {}, 0.42});
    }

    for (Spark& p : trail)
        p.life -= dt;

    std::erase_if(trail, [](const Spark& p) {
        return p.life <= 0;
    });

    if (!resting && drag == Drag::None &&
        std::fmod(age, 48.0) > 40 &&
        !returning)
    {
        returning = true;
    }
}

void Youxi::draw(Scene& scene, const World& w) const {
    double s = w.scale;

    Color rim = color(
        w.palette == 1 ? 0xA7A5EF :
        w.palette == 2 ? 0xF0C181 : 0xAFE4D6,
        0.92f);

    Color inner = color(
        w.palette == 1 ? 0x53549C :
        w.palette == 2 ? 0xAD7652 : 0x468796,
        0.66f);

    for (const Spark& p : trail) {
        float alpha = static_cast<float>(p.life * 0.13);

        scene.ring(
            p.p,
            58 * s,
            37 * s,
            color(0x9ACCCB, alpha),
            4 * s);
    }

    drawNest(scene, w);

    for (const Echo& echo : echoes) {
        float alpha = static_cast<float>(
            std::clamp(echo.life / 2, 0.0, 0.7));

        scene.ring(
            echo.p,
            echo.radius * s,
            echo.radius * 0.64 * s,
            color(0xB7DCD6, alpha),
            1.6 * s);

        scene.ring(
            echo.p,
            echo.radius * 0.86 * s,
            echo.radius * 0.56 * s,
            color(0x8DBDC8, alpha * 0.35f),
            2.2 * s);
    }

    for (int i = 0; i + 1 < membraneCount; ++i) {
        double u = static_cast<double>(i) / (membraneCount - 1);
        double innerScale = 0.65 + std::sin(u * PI) * 0.09;

        V a = center + (membrane[i] - center) * innerScale;
        V b = center + (membrane[i + 1] - center) * innerScale;

        Color top = rim;
        top.a = 0.45f + static_cast<float>(std::sin(u * PI) * 0.2);

        scene.poly(
            {membrane[i], membrane[i + 1], b, a},
            top,
            inner);

        scene.line(
            membrane[i], membrane[i + 1],
            rim, 1.4 * s);

        scene.line(
            a, b,
            color(0x76AFC4, 0.65f), 0.9 * s);

        if (i % 5 == 0)
            scene.line(a, membrane[i],
                       color(0xD6E8D6, 0.16f), 0.8 * s);
    }

    scene.chain(longThread.p, color(0xB1D5C9, 0.88f), 1.8 * s);
    scene.chain(shortThread.p, color(0x85B4C0, 0.78f), 1.2 * s);

    scene.ellipse(
        nodes[0], {10 * s, 5 * s},
        color(0xF1DEA7), color(0xA2B3A3));

    scene.ellipse(
        nodes[1], {9 * s, 8 * s},
        color(0xC6E5E6), color(0x659EA9));

    scene.ellipse(
        nodes[2], {7 * s, 4 * s},
        color(0xD6BEE7), color(0x8F8AB7));

    scene.disk(
        longThread.p.back(),
        (drag == Drag::Thread ? 5.5 : 3.6) * s,
        color(0xE8D29B));

    if (throughPulse > 0.02) {
        scene.ring(
            center,
            (77 + throughPulse * 24) * s,
            (58 + throughPulse * 18) * s,
            color(0xD9EED4, static_cast<float>(throughPulse * 0.4)),
            1.5 * s);
    }
}

double Youxi::membraneAngle(int index) {
    return 0.48 +
        (TAU - 0.96) *
        static_cast<double>(index) /
        (membraneCount - 1);
}

void Youxi::chooseTarget(World& w) {
    double s = w.scale;

    const auto& surfaces = w.desktop.surfaces;

    if (!surfaces.empty()) {
        size_t index =
            static_cast<size_t>(age * 0.71) % surfaces.size();

        const Surface& surface = surfaces[index];

        double u = 0.2 + (0.5 + 0.5 * std::sin(age)) * 0.6;

        target =
            surface.at(u) +
            surface.outward * (95 * s);

        if (!w.desktop.onScreen(target)) {
            target = w.desktop.home + V{
                std::sin(age * 0.7) * 230 * s,
                std::cos(age * 0.5) * 100 * s
            };
        }
    } else {
        target = w.desktop.home;
    }

    V mouseVelocity = w.mouse - w.previousMouse;

    if (dist(w.mouse, center) < 220 * s &&
        len(mouseVelocity) > 1)
    {
        target = w.mouse + unit(mouseVelocity) * (75 * s);
    }
}

void Youxi::updateDisturbance(World& w, double dt) {
    disturbance *= std::exp(-4 * dt);

    if (observing) {
        if (const Surface* next =
                w.desktop.resolve(observedSurface))
        {
            V delta = next->a - observedSurface.a;

            if (len(delta) > 0.1 &&
                segmentDistance(center, next->a, next->b) < 260 * w.scale)
            {
                disturbance += limit(
                    delta * 8,
                    200 * w.scale);
            }

            observedSurface = *next;
        } else {
            observing = false;
        }
    }

    if (!observing) {
        if (const Surface* surface =
                w.desktop.nearest(center, 260 * w.scale))
        {
            observedSurface = *surface;
            observing = true;
        }
    }
}

void Youxi::updateEchoes(World& w, double dt) {
    for (Echo& echo : echoes) {
        echo.life -= dt;

        V mouseDelta = w.mouse - w.previousMouse;

        if (dist(w.mouse, echo.p) < echo.radius * w.scale + 16 * w.scale)
            echo.velocity += limit(mouseDelta * 5, 80 * w.scale);

        if (echo.life < 1.2) {
            echo.velocity += (center - echo.p) * (6 * dt);
            echo.velocity *= std::exp(-2 * dt);
        }

        echo.p += echo.velocity * dt;
    }

    std::erase_if(echoes, [](const Echo& echo) {
        return echo.life <= 0;
    });
}

void Youxi::drawNest(Scene& scene, const World& w) const {
    if (weave <= 0.001) return;

    double s = w.scale;

    for (int strand = 0; strand < 4; ++strand) {
        int count = std::max(1, static_cast<int>(32 * weave));

        V previous{};

        for (int i = 0; i <= count; ++i) {
            double u = static_cast<double>(i) / 32;

            double x = (u - 0.5) * (111 - strand * 9) * s;

            double y =
                std::sin(u * PI) * (15 + strand * 5) * s +
                (strand - 1.5) * 4 * s;

            y += std::sin(u * TAU * 2 + age * 11 + strand) *
                 std::sin(u * PI) * nestWave * 7 * s;

            V p = nest + rotate({x, y}, -0.12 + strand * 0.035);

            if (i > 0)
                scene.line(previous, p,
                           color(0xA4CBC2, 0.58f), 1.1 * s);

            previous = p;
        }
    }

    scene.disk(nest, 3.5 * s, color(0xD1DCC4, 0.82f));
}

} // namespace pets
