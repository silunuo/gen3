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

    for (int i = 0; i < 3; ++i) {
        if (dist(p, nodes[i]) < 14 * s) {
            nodeVelocity[i] += unit(p - nodes[i], {0, -1}) * (-180 * s);
            w.audio.play(1, 500 + i * 90, 0.06);
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

    if (dist(p, shortThread.p.back()) < 16 * s) {
        drag = Drag::ShortThread;
        offset = shortThread.p.back() - p;
        resting = false;
        returning = false;
        w.audio.play(1, 460, 0.07);
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
            // 轻点和拖拽分开判定，真正提升为拖拽前先记下按下的位置/时间。
            pendingNestGrab = true;
            nestDownPoint = p;
            nestDownAge = age;
        } else {
            // 拨动巢丝：只是轻轻晃一下，不会让游隙自己飞回巢。
            nestWave = std::max(nestWave, 0.5);
            w.audio.play(1, 420 + std::abs(p.x - nest.x) * 3, 0.08);
        }

        return true;
    }

    return false;
}

void Youxi::up(World& w) {
    if (pendingNestGrab) {
        // 一直没被提升为真拖拽：这是轻点巢中心，触发自主归巢邀请。
        pendingNestGrab = false;

        nestWave = 1;
        resting = false;
        target = nest + V{0, -35 * w.scale};
        returning = true;

        w.audio.play(1, 420, 0.12);
    }

    if (drag == Drag::Body) {
        triggerBloom(w);
        velocity = limit(velocity, 650 * w.scale);
        target = center + velocity * 0.45;
        targetClock = 1.5;

        w.audio.play(1, 520, 0.08);
    }

    if (drag == Drag::Thread) {
        triggerBloom(w);
        target = center + velocity * 0.5;
        targetClock = 2;

        w.audio.play(1, 720, 0.09);
    }

    if (drag == Drag::ShortThread) {
        triggerBloom(w, 0.6);
        target = center + velocity * 0.3;
        targetClock = 1.2;

        w.audio.play(1, 820, 0.07);
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
    releaseBloom *= std::exp(-1.8 * dt);
    throughPulse *= std::exp(-3.4 * dt);
    chaseGraceClock -= dt;

    longThread.segment = 8 * s;
    shortThread.segment = 6 * s;

    updateDisturbance(w, dt);
    updateEchoes(w, dt);

    if (pendingNestGrab) {
        bool promote =
            dist(w.mouse, nestDownPoint) > 14 * s ||
            (age - nestDownAge) > 0.18;

        if (promote) {
            pendingNestGrab = false;
            drag = Drag::Nest;
            offset = nest - w.mouse;
        }
    }

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
    } else if (drag == Drag::ShortThread) {
        V pin = w.mouse + offset;

        V orbit = pin + V{
            std::sin(age * 2.1) * 48 * s,
            -48 * s + std::cos(age * 1.8) * 16 * s
        };

        spring(center, velocity, orbit, 40, 14, dt);

        V separation = center - pin;
        double maximum = 70 * s;

        if (len(separation) > maximum) {
            center = pin + unit(separation) * maximum;

            V radial = unit(separation);
            double outwardVelocity = dot(velocity, radial);

            if (outwardVelocity > 0)
                velocity -= radial * outwardVelocity;
        }
    } else {
        if (drafting) {
            // 借风绕游：扰流经过后绕着扰流点转一圈，再回原位。
            draftTimer -= dt;
            target = draftAnchor + polar(age * 2.4) * (70 * s);

            if (draftTimer <= 0) {
                drafting = false;
                target = draftHome;
                targetClock = 1.2;
            }
        } else if (chasingEcho) {
            // 捕捉回声：追上自己留下的一个残影点，追到就合住消失。
            target = chaseTarget;

            if (dist(center, chaseTarget) < 22 * s) {
                compact = std::max(compact, 0.5);

                std::erase_if(trail, [&](const Spark& p) {
                    return dist(p.p, chaseTarget) < 6 * s;
                });

                chasingEcho = false;
                chaseGraceClock = 16 + std::sin(age * 0.6) * 4;
                targetClock = 0;
            }
        } else if (fishing) {
            fishingTimer -= dt;
            target = fishingAnchor + V{0, std::sin(age * 2.2) * 10 * s};

            fishClock -= dt;

            if (fishClock <= 0) {
                fishClock = 1.4;

                if (echoes.size() < 3) {
                    echoes.push_back({
                        fishingAnchor, V{0, -10 * s}, 14, 1.0
                    });
                }
            }

            if (fishingTimer <= 0) fishing = false;
        } else if (targetClock <= 0 && !returning && !resting) {
            if (chaseGraceClock <= 0 && trail.size() >= 2 &&
                std::sin(age * 0.47) > 0.93)
            {
                chasingEcho = true;
                chaseTarget = trail.front().p;
                targetClock = 6;
            } else {
                chooseTarget(w);
                targetClock = 4.5 + std::sin(age * 0.4) * 1.2;
            }
        }

        if (resting) {
            nestPluckClock -= dt;

            if (nestPluckClock <= 0) {
                nestPluckClock = 8 + std::sin(age * 0.37) * 4;
                nestWave = std::max(nestWave, 0.4);
                w.audio.play(1, 400, 0.05);
            }
        }

        if (returning) {
            target = nest + V{0, -22 * s};

            if (dist(center, target) < 20 * s) {
                weave = std::min(1.0, weave + dt * 0.34);

                if (weave >= 1) {
                    resting = true;
                    returning = false;
                    sleepAge = 0;

                    for (int i = 0; i < 4; ++i) {
                        double a = static_cast<double>(i) / 4 * TAU + age;

                        motes.push_back({
                            nest + polar(a) * (12 * s),
                            polar(a) * (24 * s),
                            0.5
                        });
                    }
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
        ((resting || returning) ? 1.0 : 0.0) - fold
    ) * follow(2.4, dt);

    double speed = len(velocity);
    V desiredFacing = unit(velocity, facing);

    facing = unit(mix(
        facing, desiredFacing, follow(2.9, dt)));

    double rotation = std::atan2(facing.y, facing.x) * 0.22;
    double stretch = std::clamp(speed / (450 * s), 0.0, 0.32);

    // 七种命名体态：按优先级每帧重新判一次，几何效果靠下面这些
    // 缓动量追赶目标值，过渡天然平滑，不需要额外的转场状态机。
    {
        double facingAngle = std::atan2(facing.y, facing.x);
        double instantTurnRate =
            wrapAngle(facingAngle - previousFacingAngle) / std::max(1e-4, dt);

        turnRate += (instantTurnRate - turnRate) * follow(6, dt);
        previousFacingAngle = facingAngle;

        if (drag == Drag::Body) posture = Posture::ClosedPetal;
        else if (releaseBloom > 0.25) posture = Posture::Bloom;
        else if (resting || returning) posture = Posture::Pouch;
        else if (drag == Drag::Thread || drag == Drag::ShortThread ||
                 len(disturbance) > 40 * s) posture = Posture::Sail;
        else if (std::abs(turnRate) > 1.8) posture = Posture::Crescent;
        else if (speed > 220 * s) posture = Posture::FoldedShuttle;
        else posture = Posture::OpenRing;

        compact += (
            (posture == Posture::ClosedPetal ? 1.0 : 0.0) - compact
        ) * follow(10, dt);

        sailBoost += (
            (posture == Posture::Sail ? 1.0 : 0.0) - sailBoost
        ) * follow(7, dt);

        shuttleBoost += (
            (posture == Posture::FoldedShuttle ? 1.0 : 0.0) - shuttleBoost
        ) * follow(8, dt);

        double crescentTarget = posture == Posture::Crescent
            ? std::clamp(turnRate / 3.0, -1.0, 1.0) : 0.0;

        crescentBias += (crescentTarget - crescentBias) * follow(8, dt);
    }

    stretch = std::min(0.62, stretch + shuttleBoost * 0.3);

    // 穿环：鼠标靠近中心时，身体缺口慢慢转去对准鼠标；离开后松开。
    {
        V toHole = w.mouse - center;
        double holeDist = len(toHole);

        double approachTarget = (drag == Drag::None)
            ? std::clamp(1.0 - holeDist / (170 * s), 0.0, 1.0) : 0.0;

        holeApproach += (approachTarget - holeApproach) * follow(5, dt);

        double desiredBias = holeApproach > 0.05
            ? wrapAngle(std::atan2(toHole.y, toHole.x) - rotation) *
                holeApproach
            : 0.0;

        holeFaceBias += wrapAngle(desiredBias - holeFaceBias) * follow(4, dt);
        rotation += holeFaceBias;

        if (inviting)
            rotation += std::sin(age * 1.6) * 0.35;

        V local = rotate(w.mouse - center, -rotation);
        double rx = 0.65 * 77 * s, ry = 0.65 * 59 * s;

        bool inHoleNow =
            (local.x * local.x) / (rx * rx) +
            (local.y * local.y) / (ry * ry) < 1.0;

        if (!inHoleNow && mouseInHole) triggerThread(w);
        mouseInHole = inHoleNow;
    }

    threadWindowClock -= dt;
    if (threadWindowClock <= 0) threadCount = 0;

    threadPulse *= std::exp(-2.2 * dt);

    for (int i = 0; i < 3; ++i) {
        if (nodeThreadDelay[i] < 0) continue;

        nodeThreadDelay[i] -= dt;

        if (nodeThreadDelay[i] <= 0) {
            nodeVelocity[i] += unit(nodes[i] - center, {0, -1}) * (140 * s);
            nodeThreadDelay[i] = -1;
        }
    }

    if (inviting) {
        inviteClock -= dt;
        targetClock = 1;
        target = center;

        if (inviteClock <= 0) {
            inviting = false;
            threadCount = 0;
            targetClock = 0;
        }
    } else if (
        threadCount >= 3 && drag == Drag::None &&
        !resting && !returning && !fishing &&
        std::sin(age * 0.31) > 0.94)
    {
        inviting = true;
        inviteClock = 2.5;
    }

    for (Spark& m : motes) {
        m.life -= dt;
        m.velocity *= std::exp(-3 * dt);
        m.p += m.velocity * dt;
    }

    std::erase_if(motes, [](const Spark& m) { return m.life <= 0; });

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

    // 引导节点：先探向身体正要去的方向，不止是盯着鼠标。
    nodeTargets[0] += unit(velocity, facing) *
        (std::clamp(speed / (300 * s), 0.0, 1.0) * 20 * s);

    // 平衡节点：转弯时向外侧展开，而不是固定半径匀速公转。
    nodeTargets[1] += perp(facing) *
        (-std::clamp(turnRate / 3.0, -1.0, 1.0) * 24 * s);

    if (fold > 0) {
        for (V& p : nodeTargets)
            p = mix(p, center + (p - center) * 0.4, fold);
    }

    if (compact > 0) {
        // 闭瓣：受扰/被抓时节点往身体中心贴。
        for (V& p : nodeTargets)
            p = mix(p, center + (p - center) * 0.3, compact);
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

    // 引导节点反应最快，平衡节点居中，织构节点更从容、不抢先。
    static constexpr double nodeStiffness[3] = {70, 48, 36};
    static constexpr double nodeDamping[3] = {10, 12, 14};

    for (int i = 0; i < 3; ++i) {
        spring(
            nodes[i], nodeVelocity[i], nodeTargets[i],
            nodeStiffness[i], nodeDamping[i], dt);
    }

    double pulse = std::sin(age * 3.3);
    double opening =
        1 + throughPulse * 0.18 + releaseBloom * 0.4 +
        diffusePulse * 0.16 - convergePulse * 0.16 -
        contractPulse * 0.22 + sailBoost * 0.38 - compact * 0.45 +
        threadPulse * 0.5;

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

        // 弯月：转弯时一侧明显展开、一侧明显收紧。
        double crescent =
            1 + (std::sin(a) >= 0 ? 1.0 : -1.0) * crescentBias * 0.4;
        local.y *= crescent;

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

    double nestDistance = dist(center, nest);

    if (!nestTouch && nestDistance < 70 * s) nestTouch = true;
    else if (nestTouch && nestDistance > 95 * s) nestTouch = false;

    bool touchNest = resting || (returning && nestTouch);

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

    // draw() 是 const，拿不到这些局部量，缓存下来给渲染用。
    renderStretch = stretch;
    renderMouseNear = mouseNear;
}

void Youxi::draw(Scene& scene, const World& w) const {
    double s = w.scale;

    if (const Surface* ground = w.desktop.groundAt(center.x)) {
        double lift = std::max(0.0, ground->a.y - center.y);
        double strength = std::clamp(1.0 - lift / (240 * s), 0.0, 0.5);
        double scaleDown = 1.0 - 0.45 * std::clamp(lift / 240.0, 0.0, 1.0);

        contactShadow(
            scene, *ground, ground->at(ground->parameter(center)),
            30 * s * scaleDown, 9 * s * scaleDown, strength);
    }

    Color rim = color(
        w.palette == 1 ? 0xA7A5EF :
        w.palette == 2 ? 0xF0C181 : 0xAFE4D6,
        0.92f);

    Color inner = color(
        w.palette == 1 ? 0x53549C :
        w.palette == 2 ? 0xAD7652 : 0x468796,
        0.66f);

    // 虹彩边缘：拉伸越大，边缘越往另一个色相偏。
    Color stretchedRim = color(
        w.palette == 1 ? 0xE2A7D9 :
        w.palette == 2 ? 0xF7E2A0 : 0xD7F0E4,
        0.92f);

    float rimShift = static_cast<float>(
        std::clamp(renderStretch / 0.62, 0.0, 1.0));

    rim.r += (stretchedRim.r - rim.r) * rimShift;
    rim.g += (stretchedRim.g - rim.g) * rimShift;
    rim.b += (stretchedRim.b - rim.b) * rimShift;

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

    for (const Spark& m : motes) {
        float alpha = static_cast<float>(std::clamp(m.life / 0.6, 0.0, 1.0));
        scene.disk(m.p, 2.2 * s * alpha, color(0xE8F2E4, alpha * 0.85f));
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

        // 分层半透明：内缘再叠一条更窄更暗的带，制造厚度感。
        double deepScale = innerScale * 0.82;
        V da = center + (membrane[i] - center) * deepScale;
        V db = center + (membrane[i + 1] - center) * deepScale;

        scene.poly(
            {a, b, db, da},
            color(0x25303B, 0.16f),
            color(0x25303B, 0.04f));

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

    {
        // 节点光泽：引导节点靠近鼠标时更亮。
        Color guideFill = color(0xF1DEA7);
        float gloss = static_cast<float>(renderMouseNear * 0.6);

        guideFill.r += (1.0f - guideFill.r) * gloss;
        guideFill.g += (1.0f - guideFill.g) * gloss;
        guideFill.b += (1.0f - guideFill.b) * gloss;

        scene.ellipse(
            nodes[0], {10 * s, 5 * s},
            guideFill, color(0xA2B3A3));
    }

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

    scene.disk(
        shortThread.p.back(),
        (drag == Drag::ShortThread ? 4.6 : 3.0) * s,
        color(0xCDE0B8));

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

void Youxi::triggerBloom(World& w, double strength) {
    releaseBloom = strength;

    // 翻花：由内向外炸开一下，而不是只靠 opening 慢慢撑大。
    for (int i = 0; i < membraneCount; ++i) {
        membraneVelocity[i] +=
            unit(membrane[i] - center, {1, 0}) * (180 * w.scale * strength);
    }
}

void Youxi::triggerThread(World& w) {
    double s = w.scale;

    threadPulse = 1;
    threadCount++;
    threadWindowClock = 10;

    nodeThreadDelay = {0.0, 0.12, 0.24};

    // 主膜翻展：冲量从缺口一侧最强，向对侧线性衰减，靠膜点间已有的
    // 弹簧耦合自然传开，看起来像一个从缺口开始的翻展波。
    for (int i = 0; i < membraneCount; ++i) {
        double a = membraneAngle(i);
        double u = (a - 0.48) / (TAU - 0.96);

        membraneVelocity[i] +=
            unit(membrane[i] - center, {1, 0}) * (220 * s * (1 - u));
    }

    for (int i = 0; i < 5; ++i) {
        double a = static_cast<double>(i) / 5 * TAU + age;

        motes.push_back({
            center + polar(a) * (20 * s),
            polar(a) * (60 * s),
            0.6
        });
    }

    w.audio.play(1, 540, 0.07);
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
        } else if (std::sin(age * 0.13) > 0.6) {
            // 边缘垂钓：偶尔不只是路过，在这个边缘附近停下来待一阵。
            fishing = true;
            fishingAnchor = target;
            fishingTimer = 5 + std::sin(age * 0.5) * 2;
            fishClock = 0;
        }
    } else {
        target = w.desktop.home;
    }

    V mouseVelocity = w.mouse - w.previousMouse;

    if (dist(w.mouse, center) < 220 * s &&
        len(mouseVelocity) > 1)
    {
        fishing = false;
        target = w.mouse + unit(mouseVelocity) * (75 * s);
    }
}

void Youxi::updateDisturbance(World& w, double dt) {
    disturbance *= std::exp(-4 * dt);
    contractPulse *= std::exp(-5 * dt);
    diffusePulse *= std::exp(-3.6 * dt);
    convergePulse *= std::exp(-3.6 * dt);

    if (observing) {
        if (const Surface* next =
                w.desktop.resolve(observedSurface))
        {
            V delta = next->a - observedSurface.a;

            if (len(delta) > 0.1 &&
                segmentDistance(center, next->a, next->b) < 260 * w.scale)
            {
                // 借流：慢经过只是轻微偏移/侧滑；快经过先收缩一下再滑开，
                // 用同一个位移量区分，不另外判断"这算慢还是快"。
                double passSpeed = len(delta) / std::max(1e-4, dt);

                if (passSpeed > 220 * w.scale) {
                    contractPulse = std::min(1.0, contractPulse + 0.6);

                    if (!drafting && drag == Drag::None && !fishing &&
                        !chasingEcho && !resting && !returning &&
                        std::sin(age * 0.53) > 0.92)
                    {
                        drafting = true;
                        draftHome = center;
                        draftAnchor =
                            center + unit(delta, {1, 0}) * (90 * w.scale);
                        draftTimer = 2.6;
                    }
                }

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

    // 开关窗口：附近新出现一扇窗就局部扩散一下，消失则局部收束一下。
    if (w.desktop.recentWindowOpened(center, 320 * w.scale, 1.0))
        diffusePulse = std::min(1.0, diffusePulse + 0.5);

    if (w.desktop.recentWindowClosed(center, 320 * w.scale, 1.0))
        convergePulse = std::min(1.0, convergePulse + 0.5);
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
