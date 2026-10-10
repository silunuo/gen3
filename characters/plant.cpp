#include "plant.hpp"

#include <cmath>

namespace pets {

void Plant::initialize(World& w, const Settings& settings) {
    root = settings.position(
        L"Plant",
        {w.desktop.home.x, w.desktop.home.y});

    if (!w.desktop.onScreen(root))
        root = w.desktop.home;

    age = settings.get(L"Plant", L"age", 0);
    spread = settings.get(L"Plant", L"spread", 1);
    desiredSpread = spread;

    if (const Surface* floor = w.desktop.groundAt(root.x)) {
        rootSupport = *floor;
        rootU = floor->parameter(root);
        rootAttached = true;
        root = floor->at(rootU) + V{0, -29 * w.scale};
    }

    for (int branchIndex = 0; branchIndex < 2; ++branchIndex) {
        Branch& branch = branches[branchIndex];

        const wchar_t* section =
            branchIndex == 0 ? L"PlantBranch0" : L"PlantBranch1";

        int count = static_cast<int>(
            settings.get(section, L"count",
                branchIndex == 0 ? 18 : 15));

        count = std::clamp(count, 10, 24);
        branch.points.resize(static_cast<size_t>(count));

        branch.phase = settings.get(
            section, L"phase", branchIndex * 0.29);

        branch.growth = std::clamp(
            settings.get(section, L"growth", 1), 0.3, 1.0);

        double side = branchIndex == 0 ? 1 : -1;

        branch.tip = root + V{
            side * 150 * w.scale,
            -(branchIndex == 0 ? 263 : 210) * w.scale
        };

        for (size_t i = 0; i < branch.points.size(); ++i) {
            double u = static_cast<double>(i) /
                       static_cast<double>(branch.points.size() - 1);

            V fallback =
                mix(root, branch.tip, u) +
                V{side * std::sin(u * PI) * 42 * w.scale,
                  -std::sin(u * PI) * 20 * w.scale};

            std::wstring keyX = L"px" + std::to_wstring(i);
            std::wstring keyY = L"py" + std::to_wstring(i);

            branch.points[i] = root + V{
                settings.get(section, keyX.c_str(), fallback.x - root.x),
                settings.get(section, keyY.c_str(), fallback.y - root.y)
            };
        }

        branch.tip = branch.points.back();
    }
}

bool Plant::down(V p, World& w) {
    double s = w.scale;

    for (Seed& seed : seeds) {
        if (dist(p, seed.p) < 18 * s) {
            seed.velocity += V{
                std::sin(age * 3) * 100 * s,
                -100 * s
            };

            w.audio.play(2, 890, 0.05);
            return true;
        }
    }

    for (Drop& drop : drops) {
        if (dist(p, drop.p) < 14 * s) {
            dragDrop = &drop - drops.data();
            dragPart = 3;
            offset = drop.p - p;
            return true;
        }
    }

    if (dist(p, root) < 42 * s) {
        dragPart = 0;
        offset = root - p;
        rootAttached = false;
        rootVelocity = 0;

        for (Branch& branch : branches)
            branch.attached = false;

        return true;
    }

    for (int i = 0; i < 2; ++i) {
        Branch& branch = branches[i];

        if (dist(p, branch.points.back()) < 32 * s) {
            if (branch.phase > 0.77) {
                releaseSeeds(i, w);
                branch.phase = 0.92;
            } else {
                dragPart = i + 1;
                offset = branch.points.back() - p;
                branch.attached = false;
            }

            return true;
        }
    }

    for (int branchIndex = 0; branchIndex < 2; ++branchIndex) {
        Branch& branch = branches[branchIndex];

        for (size_t i = 2; i + 1 < branch.points.size(); i += 2) {
            V leafTip = leafGeometry(branchIndex, i, w).tip;

            if (segmentDistance(p, branch.points[i], leafTip) < 22 * s) {
                branch.touch = 1;
                branch.touchPosition = static_cast<double>(i);
                w.audio.play(2, 360 + i * 24, 0.055);
                return true;
            }
        }
    }

    return false;
}

void Plant::up(World& w) {
    double s = w.scale;

    if (dragPart == 0) {
        const Surface* surface = w.desktop.nearest(
            root + V{0, 29 * s},
            70 * s,
            true,
            90 * s);

        if (surface) {
            rootSupport = *surface;
            rootU = surface->parameter(root);
            rootAttached = true;

            translate(
                surface->at(rootU) + V{0, -29 * s} - root);
        }
    } else if (dragPart == 1 || dragPart == 2) {
        Branch& branch = branches[dragPart - 1];

        const Surface* support = w.desktop.nearest(
            branch.tip, 55 * s, false, 70 * s);

        if (support) {
            branch.support = *support;
            branch.supportU = support->parameter(branch.tip);
            branch.attached = true;
            branch.attachBlend = 0;
            branch.bridging = rootAttached && support->owner != rootSupport.owner;
            branch.release = 0;
        } else {
            branch.freeTarget = branch.tip;
            branch.freeTargetLife = 10;
        }
    }

    dragPart = -1;
    dragDrop = -1;
}

void Plant::command(int command, V, World&) {
    if (command == Gather) {
        desiredSpread = 0.46;

        for (Branch& branch : branches)
            branch.attached = false;
    } else if (command == Expand) {
        desiredSpread = 1;
        anchorClock = 0;
    }
}

void Plant::save(const Settings& settings) const {
    settings.setPosition(L"Plant", root);
    settings.set(L"Plant", L"age", age);
    settings.set(L"Plant", L"spread", desiredSpread);

    for (int branchIndex = 0; branchIndex < 2; ++branchIndex) {
        const Branch& branch = branches[branchIndex];

        const wchar_t* section =
            branchIndex == 0 ? L"PlantBranch0" : L"PlantBranch1";

        settings.set(
            section, L"count",
            static_cast<double>(branch.points.size()));

        settings.set(section, L"phase", branch.phase);
        settings.set(section, L"growth", branch.growth);

        for (size_t i = 0; i < branch.points.size(); ++i) {
            std::wstring keyX = L"px" + std::to_wstring(i);
            std::wstring keyY = L"py" + std::to_wstring(i);

            settings.set(
                section, keyX.c_str(),
                branch.points[i].x - root.x);

            settings.set(
                section, keyY.c_str(),
                branch.points[i].y - root.y);
        }
    }
}

void Plant::tick(World& w, double dt) {
    age += dt;
    double s = w.scale;

    spread += (desiredSpread - spread) * follow(1.4, dt);

    double sleepTarget =
        std::fmod(age, 94.0) > 78 ? 1.0 : 0.0;

    sleep += (sleepTarget - sleep) * follow(1.2, dt);

    if (dragPart == 0) {
        V target = w.mouse + offset;
        V next = mix(root, target, follow(22, dt));
        translate(next - root);
    } else {
        updateRoot(w, dt);
    }

    anchorClock -= dt;

    if (anchorClock <= 0 && desiredSpread > 0.7 && dragPart != 0) {
        anchorClock = 5.5;
        seekAnchors(w);
    }

    for (int branchIndex = 0; branchIndex < 2; ++branchIndex) {
        Branch& branch = branches[branchIndex];

        branch.touch *= std::exp(-1.75 * dt);
        branch.freeTargetLife -= dt;
        branch.phase += dt / 74.0;

        if (branch.phase >= 1) {
            branch.phase -= 1;
            branch.growth = 0.82;
            branch.releasedSeeds = false;
        }

        branch.growth = std::min(
            1.0, branch.growth + dt * 0.016);

        if (branch.phase > 0.82 &&
            !branch.releasedSeeds &&
            dragPart != branchIndex + 1)
        {
            releaseSeeds(branchIndex, w);
            branch.releasedSeeds = true;
        }

        double side = branchIndex == 0 ? 1 : -1;
        double height = branchIndex == 0 ? 262 : 210;

        V target = root + V{
            side * (145 + std::sin(age * 0.43 + branchIndex) * 20) *
                s * spread,

            -height * s * spread *
                (1 - sleep * 0.24)
        };

        if (branch.freeTargetLife > 0)
            target = branch.freeTarget;

        if (branch.attached) {
            const Surface* support =
                w.desktop.resolve(branch.support);

            if (!support) {
                branch.attached = false;
                branch.freeTarget = branch.tip + V{0, 35 * s};
                branch.freeTargetLife = 1.2;
            } else {
                branch.support = *support;

                V attachedTarget =
                    support->at(branch.supportU) +
                    support->outward * (10 * s);

                double reach =
                    (branch.points.size() - 1) *
                    segmentLength(branch, w);

                double stretchRatio = dist(root, attachedTarget) / reach;

                if (branch.bridging && stretchRatio <= 1.3) {
                    // 跨窗枝桥：拉远时卷须逐步解开，而不是一下子松手；
                    // 窗口再靠近的话，同一套缓动会把它重新卷回去。
                    double releaseTarget = stretchRatio > 0.82 ? 1.0 : 0.0;

                    branch.release += (
                        releaseTarget - branch.release
                    ) * follow(2, dt);

                    if (branch.release > 0.97) {
                        branch.attached = false;
                        branch.bridging = false;
                        branch.release = 0;
                        branch.freeTarget = branch.tip;
                        branch.freeTargetLife = 0.6;
                    } else {
                        branch.attachBlend = std::min(
                            1.0, branch.attachBlend + dt);

                        target = mix(
                            target,
                            attachedTarget,
                            smooth(branch.attachBlend));
                    }
                } else if (stretchRatio > 0.96) {
                    branch.attached = false;
                    branch.bridging = false;
                    branch.release = 0;
                    branch.freeTarget = branch.tip;
                    branch.freeTargetLife = 0.6;
                } else {
                    branch.attachBlend = std::min(
                        1.0, branch.attachBlend + dt);

                    target = mix(
                        target,
                        attachedTarget,
                        smooth(branch.attachBlend));
                }
            }
        }

        if (dragPart == branchIndex + 1)
            target = w.mouse + offset;

        // 相邻花序互相让位：两条枝的目标靠太近时互相推开一点。
        if (dragPart != branchIndex + 1) {
            V otherTip = branches[1 - branchIndex].points.back();
            double tooClose = 70 * s * spread;
            double gap = dist(target, otherTip);

            if (gap < tooClose && gap > 1e-3) {
                target += unit(target - otherTip) * (tooClose - gap) * 0.5;
            }
        }

        spring(
            branch.tip,
            branch.tipVelocity,
            target,
            dragPart == branchIndex + 1 ? 78 : 14,
            dragPart == branchIndex + 1 ? 18 : 8,
            dt);

        double lengthPerSegment = segmentLength(branch, w);

        for (size_t i = 1; i + 1 < branch.points.size(); ++i) {
            double u = static_cast<double>(i) /
                       static_cast<double>(branch.points.size() - 1);

            V preferred =
                mix(root, branch.tip, u) +
                V{
                    side * std::sin(u * PI) * 45 * s * spread,
                    -std::sin(u * PI) * 24 * s * spread
                };

            branch.points[i] = mix(
                branch.points[i],
                preferred,
                follow(
                    dragPart == branchIndex + 1 ? 1.6 : 3.0,
                    dt));
        }

        double reachLimit =
            lengthPerSegment * (branch.points.size() - 1) * 0.99;

        V fabrikTarget = branch.tip;

        if (dist(root, fabrikTarget) > reachLimit)
            fabrikTarget = root + unit(fabrikTarget - root) * reachLimit;

        fabrik(
            branch.points,
            root,
            fabrikTarget,
            lengthPerSegment,
            32);

        // 约束局部折角，然后再恢复长度。
        // 枝条可以弯曲，但不在单个节上折成尖锐肘部。
        for (int pass = 0; pass < 3; ++pass) {
            for (size_t i = 2; i < branch.points.size(); ++i) {
                V previousDirection =
                    unit(branch.points[i - 1] - branch.points[i - 2]);

                V currentDirection =
                    unit(branch.points[i] - branch.points[i - 1]);

                double angle = std::atan2(
                    cross(previousDirection, currentDirection),
                    dot(previousDirection, currentDirection));

                double limited = std::clamp(angle, -0.42, 0.42);

                if (std::abs(angle - limited) > 1e-5) {
                    branch.points[i] =
                        branch.points[i - 1] +
                        rotate(previousDirection, limited) *
                        lengthPerSegment;
                }
            }
        }

        branch.points.front() = root;

        updateLeafRenewal(branchIndex, w, dt);
    }

    updateDrops(w, dt);
    updateSeeds(w, dt);
    updateFallingLeaves(w, dt);
}

void Plant::draw(Scene& scene, const World& w) const {
    double s = w.scale;

    if (rootAttached) {
        contactShadow(scene, rootSupport, rootSupport.at(rootU), 34, 10, 0.8);
    }

    Color leafLight = color(
        w.palette == 1 ? 0x8AB178 :
        w.palette == 2 ? 0x99A3B0 : 0xA1C18F);

    Color leafDark = color(
        w.palette == 1 ? 0x365F46 :
        w.palette == 2 ? 0x424D65 : 0x416E59);

    for (int branchIndex = 1; branchIndex >= 0; --branchIndex) {
        const Branch& branch = branches[branchIndex];

        for (size_t i = 1; i < branch.points.size(); ++i) {
            double width =
                std::max(1.5, 6.4 - static_cast<double>(i) * 0.28) * s;

            scene.line(
                branch.points[i - 1], branch.points[i],
                color(branchIndex == 0 ? 0x77916B : 0x607B60),
                width);

            if (i % 3 == 0)
                scene.disk(branch.points[i], width * 0.6,
                           color(0xADC295));
        }

        for (size_t i = 2; i + 1 < branch.points.size(); i += 2) {
            Leaf leaf = leafGeometry(branchIndex, i, w);

            Color thisLeafLight = leafLight;
            Color thisLeafDark = leafDark;

            if (branch.renewingLeaf == static_cast<int>(i)) {
                float curl = static_cast<float>(
                    std::clamp(branch.renewalAge / 2.5, 0.0, 1.0));

                Color gold = color(0xCBA858);
                Color brown = color(0x8A6A3A);

                thisLeafLight.r += (gold.r - thisLeafLight.r) * curl;
                thisLeafLight.g += (gold.g - thisLeafLight.g) * curl;
                thisLeafLight.b += (gold.b - thisLeafLight.b) * curl;

                thisLeafDark.r += (brown.r - thisLeafDark.r) * curl;
                thisLeafDark.g += (brown.g - thisLeafDark.g) * curl;
                thisLeafDark.b += (brown.b - thisLeafDark.b) * curl;
            }

            scene.poly(
                {
                    leaf.base,
                    mix(leaf.base, leaf.upper, 0.68),
                    leaf.upper,
                    leaf.tip,
                    leaf.lower,
                    mix(leaf.base, leaf.lower, 0.72)
                },
                thisLeafLight,
                thisLeafDark);

            scene.line(
                leaf.base, leaf.tip,
                color(0xC6D3A4, 0.78f), 0.9 * s);

            scene.line(
                mix(leaf.base, leaf.tip, 0.42),
                leaf.upper,
                color(0xB8CAA0, 0.46f), 0.65 * s);

            scene.line(
                mix(leaf.base, leaf.tip, 0.58),
                leaf.lower,
                color(0xB8CAA0, 0.40f), 0.65 * s);

            if (i == 6 || i == 10)
                drawTendril(scene, branchIndex, i, w);
        }

        drawFlower(scene, branch, branchIndex, w);

        if (branch.attached) {
            V anchor = branch.support.at(branch.supportU);
            V previous = branch.points.back();

            double coil = 1 - branch.release;
            double turns = branch.bridging ? 2.4 : 2.0;
            double amplitude = (branch.bridging ? 10 : 8) * s * coil;

            Color bridgeColor = color(
                0xC9D9A0,
                static_cast<float>(1 - branch.release * 0.3));

            for (int i = 1; i <= 28; ++i) {
                double q = static_cast<double>(i) / 28;
                double angle = q * TAU * turns;

                V p = mix(branch.points.back(), anchor, q) +
                    polar(angle) * (std::sin(q * PI) * amplitude);

                scene.line(
                    previous, p,
                    branch.bridging ? bridgeColor : color(0xACBB84),
                    branch.bridging ? 1.4 * s : 1.1 * s);

                previous = p;
            }
        }
    }

    scene.ellipse(
        root + V{0, 7 * s},
        {35 * s, 25 * s},
        color(0xC4BFA0),
        color(0x80795E));

    scene.ellipse(
        root + V{-7 * s, 0},
        {20 * s, 18 * s},
        color(0xD4CEAE, 0.68f),
        color(0xA6A081, 0.55f));

    for (int i = -2; i <= 2; ++i) {
        scene.line(
            root + V{i * 8.0 * s, -10 * s},
            root + V{i * 5.6 * s, 23 * s},
            color(0x7F7B60, 0.64f),
            1.1 * s);

        V start = root + V{i * 8.0 * s, 26 * s};
        V end = root + V{i * 13.0 * s, 31 * s};

        scene.line(start, end, color(0x958B68), 1.5 * s);
    }

    for (const Drop& drop : drops) {
        scene.ellipse(
            drop.p,
            {4.5 * s, 6 * s},
            color(0xD8F5ED, 0.84f),
            color(0x6FADB1, 0.72f));

        scene.disk(
            drop.p + V{-1.4 * s, -2 * s},
            1.3 * s,
            color(0xFFFFFF, 0.94f));
    }

    for (const Seed& seed : seeds) {
        V axis = polar(seed.rotation);
        V side = perp(axis);

        scene.poly(
            {
                seed.p,
                seed.p + axis * (24 * s) + side * (7 * s),
                seed.p + axis * (36 * s),
                seed.p + axis * (17 * s) - side * (5 * s)
            },
            color(0xD9D3AA, 0.7f),
            color(0xA6AA82, 0.25f));

        scene.line(
            seed.p,
            seed.p + axis * (34 * s),
            color(0xB3B28B, 0.8f), 0.8 * s);

        scene.ellipse(
            seed.p,
            {3.5 * s, 5 * s},
            color(0xA6966C));

        if (seed.caught && seed.caughtAge < 1.2) {
            double sprout = std::clamp(seed.caughtAge / 0.5, 0.0, 1.0);

            scene.poly(
                {
                    seed.p,
                    seed.p + V{-5 * s, -9 * s * sprout},
                    seed.p + V{5 * s, -9 * s * sprout}
                },
                color(0x9FCB7E, 0.85f));
        }
    }

    for (const FallingLeaf& leaf : fallingLeaves) {
        V axis = polar(leaf.rotation);
        V side = perp(axis);

        float alpha = static_cast<float>(std::clamp(leaf.life / 1.2, 0.0, 1.0));

        scene.poly(
            {
                leaf.p - axis * (9 * s),
                leaf.p + side * (6 * s),
                leaf.p + axis * (9 * s),
                leaf.p - side * (6 * s)
            },
            color(0xCBA858, alpha),
            color(0x8A6A3A, alpha));
    }
}

double Plant::segmentLength(const Branch& branch, const World& w) const {
    return 21.5 * w.scale * spread * branch.growth;
}

void Plant::translate(V delta) {
    root += delta;

    for (Branch& branch : branches) {
        branch.tip += delta;
        branch.freeTarget += delta;

        for (V& p : branch.points)
            p += delta;
    }

    for (Drop& drop : drops) {
        if (!drop.falling)
            drop.p += delta;
    }
}

void Plant::updateRoot(World& w, double dt) {
    double s = w.scale;

    if (rootAttached) {
        const Surface* support =
            w.desktop.resolve(rootSupport);

        if (support) {
            rootSupport = *support;

            V target =
                support->at(rootU) + V{0, -29 * s};

            translate(target - root);
            rootVelocity = 0;
            return;
        }

        rootAttached = false;

        for (Branch& branch : branches)
            branch.attached = false;
    }

    double oldBottom = root.y + 29 * s;

    rootVelocity += 650 * s * dt;
    translate({0, rootVelocity * dt});

    double newBottom = root.y + 29 * s;

    const Surface* landing = nullptr;
    double nearestY = 1e30;

    for (const Surface& surface : w.desktop.surfaces) {
        if (surface.edge != 0 && !surface.ground) continue;
        if (root.x < surface.a.x + 35 * s ||
            root.x > surface.b.x - 35 * s)
            continue;

        bool crossed =
            oldBottom <= surface.a.y &&
            newBottom >= surface.a.y;

        bool belowFloor =
            surface.ground && newBottom >= surface.a.y;

        if ((crossed || belowFloor) && surface.a.y < nearestY) {
            nearestY = surface.a.y;
            landing = &surface;
        }
    }

    if (landing) {
        rootSupport = *landing;
        rootU = landing->parameter(root);
        rootAttached = true;
        rootVelocity = 0;

        translate(
            landing->at(rootU) + V{0, -29 * s} - root);

        w.audio.play(2, 240, 0.07);
    }
}

void Plant::seekAnchors(World& w) {
    for (int i = 0; i < 2; ++i) {
        Branch& branch = branches[i];

        if (branch.attached || dragPart == i + 1)
            continue;

        const Surface* best = nullptr;
        V bestPoint{};
        double score = 95 * w.scale;
        bool bestBridge = false;

        double reach =
            (branch.points.size() - 1) *
            segmentLength(branch, w);

        for (const Surface& surface : w.desktop.surfaces) {
            if (surface.ground) continue;

            V p = closestOnSegment(
                branch.points.back(),
                surface.a,
                surface.b);

            if (dist(root, p) > reach * 0.9)
                continue;

            if (dist(root, p) < 90 * w.scale)
                continue;

            double d = dist(branch.points.back(), p);
            bool bridge = rootAttached && surface.owner != rootSupport.owner;

            // 跨窗枝桥：两扇窗之间搭桥比单纯贴着自己这扇窗更值得做，
            // 给跨窗候选一个分数加成，而不是单纯选最近的。
            if (bridge) d *= 0.6;

            if (d < score) {
                score = d;
                best = &surface;
                bestPoint = p;
                bestBridge = bridge;
            }
        }

        if (best) {
            branch.support = *best;
            branch.supportU = best->parameter(bestPoint);
            branch.attached = true;
            branch.attachBlend = 0;
            branch.bridging = bestBridge;
            branch.release = 0;
        }
    }
}

Plant::Leaf Plant::leafGeometry(
    int branchIndex,
    size_t index,
    const World& w) const
{
    const Branch& branch = branches[branchIndex];
    double s = w.scale;

    V tangent = unit(
        branch.points[index + 1] -
        branch.points[index - 1]);

    double side = ((index / 2) & 1) ? 1 : -1;
    V outward = perp(tangent) * side;

    double wave =
        std::max(0.0, std::sin(
            age * 7 -
            std::abs(static_cast<double>(index) - branch.touchPosition) *
                0.6)) *
        branch.touch;

    double opening =
        std::clamp(
            1 - wave * 0.73 - sleep * 0.55,
            0.15, 1.0);

    double size =
        (42 + std::sin(index * 1.3) * 5) *
        s * branch.growth;

    // 叶片更新：这片叶子正在变黄卷曲，逐渐收起再脱落。
    if (branch.renewingLeaf == static_cast<int>(index)) {
        double curl = std::clamp(branch.renewalAge / 2.5, 0.0, 1.0);
        opening *= 1 - curl * 0.85;
        size *= 1 - curl * 0.3;
    }

    V base = branch.points[index];
    V tip =
        base + outward * (size * opening) +
        tangent * (12 * s);

    V middle = mix(base, tip, 0.53);

    return {
        base,
        tip,
        middle + tangent * (17 * opening * s),
        middle - tangent * (14 * opening * s)
    };
}

V Plant::tendrilPoint(
    int branchIndex, size_t index, double u, const World& w) const
{
    const Branch& branch = branches[branchIndex];

    V base = branch.points[index];
    V tangent = unit(
        branch.points[index + 1] -
        branch.points[index - 1]);

    V outward = perp(tangent) *
        (((index / 2) & 1) ? 1.0 : -1.0);

    double a = u * PI * 3 + std::sin(age * 0.7 + index) * 0.23;
    double radius = 10 * u * w.scale;

    return base +
        outward * (u * 28 * w.scale + std::cos(a) * radius) +
        tangent * (std::sin(a) * radius);
}

V Plant::tendrilTip(int branchIndex, size_t index, const World& w) const {
    return tendrilPoint(branchIndex, index, 1.0, w);
}

void Plant::drawTendril(
    Scene& scene,
    int branchIndex,
    size_t index,
    const World& w) const
{
    V previous = branches[branchIndex].points[index];

    for (int j = 1; j <= 24; ++j) {
        double u = j / 24.0;
        V p = tendrilPoint(branchIndex, index, u, w);

        scene.line(
            previous, p,
            color(0xB0BE89), 0.9 * w.scale);

        previous = p;
    }
}

void Plant::drawFlower(
    Scene& scene,
    const Branch& branch,
    int branchIndex,
    const World& w) const
{
    double s = w.scale;
    V center = branch.points.back();

    double opening;

    if (branch.phase < 0.18)
        opening = 0.17 + branch.phase / 0.18 * 0.18;
    else if (branch.phase < 0.4)
        opening = 0.35 + smooth((branch.phase - 0.18) / 0.22) * 0.65;
    else if (branch.phase < 0.7)
        opening = 1;
    else
        opening = std::max(0.12, 1 - (branch.phase - 0.7) / 0.2);

    opening *= 1 - sleep * 0.4;

    double attention =
        std::clamp(
            1 - dist(w.mouse, center) / (95 * s),
            0.0, 1.0);

    // 托花：半开的花被悬停会开得更足；盛开的花被悬停会微微上托。
    if (branch.phase >= 0.18 && branch.phase < 0.4)
        opening = std::min(1.0, opening + attention * 0.18);

    double lift = (branch.phase >= 0.4 && branch.phase < 0.7)
        ? attention * 4 * s : 0.0;

    V flowerCenter =
        center + unit(w.mouse - center) * (attention * 5 * s) -
        V{0, lift};

    Color petalLight = color(
        w.palette == 1 ? 0xF0C1A0 :
        w.palette == 2 ? 0xD4C7EB : 0xEAE8C9,
        0.94f);

    Color petalDark = color(
        w.palette == 1 ? 0xBA8172 :
        w.palette == 2 ? 0x9B8FAA : 0xBBC6A4,
        0.88f);

    for (int i = 0; i < 5; ++i) {
        double a = i * TAU / 5 - PI * 0.5 + branchIndex * 0.2;

        V radial = polar(a);
        V side = perp(radial);

        double size = 28 * opening * s;

        scene.poly(
            {
                flowerCenter,
                flowerCenter + radial * (size * 0.54) + side * (size * 0.42),
                flowerCenter + radial * size,
                flowerCenter + radial * (size * 0.54) - side * (size * 0.42)
            },
            petalLight,
            petalDark);

        scene.line(
            flowerCenter,
            flowerCenter + radial * (size * 0.89),
            color(0xA4AA87, 0.5f), 0.7 * s);
    }

    scene.disk(
        flowerCenter,
        (branch.phase > 0.77 ? 7 : 5) * s,
        color(branch.phase > 0.77 ? 0xA49A67 : 0xE3BF78));

    if (branch.phase > 0.77) {
        for (int i = 0; i < 3; ++i) {
            V axis = polar(age * 0.3 + i * TAU / 3);

            scene.poly(
                {
                    flowerCenter,
                    flowerCenter + axis * (21 * s) + perp(axis) * (5 * s),
                    flowerCenter + axis * (29 * s)
                },
                color(0xDAD8B5, 0.8f));
        }
    }
}

void Plant::releaseSeeds(int branchIndex, World& w) {
    V origin = branches[branchIndex].points.back();

    for (int i = 0; i < 3; ++i) {
        seeds.push_back({
            origin,
            V{(i - 1) * 27.0, -22 - i * 7.0} * w.scale,
            age + i * 2.1,
            8 + i
        });
    }

    w.audio.play(2, 700, 0.065);
}

void Plant::updateSeeds(World& w, double dt) {
    static constexpr size_t tendrilIndices[2] = {6, 10};

    for (Seed& seed : seeds) {
        if (seed.caught) {
            seed.caughtAge += dt;
            continue;
        }

        seed.life -= dt;
        seed.rotation += dt * 5.5;

        seed.velocity.x +=
            std::sin(age * 2 + seed.rotation) * 8 * w.scale * dt;

        seed.velocity.y += 17 * w.scale * dt;
        seed.velocity *= std::exp(-0.5 * dt);

        if (dist(seed.p, w.mouse) < 38 * w.scale) {
            seed.velocity += limit(
                (w.mouse - w.previousMouse) * 3,
                65 * w.scale);
        }

        if (seed.life < 2.5) {
            seed.velocity +=
                (root - seed.p) * (1.6 * dt);
        }

        seed.p += seed.velocity * dt;

        // 拨种：卷须尝试接住飞过附近的种子。
        for (int b = 0; b < 2 && !seed.caught; ++b) {
            const Branch& branch = branches[b];

            for (size_t idx : tendrilIndices) {
                if (idx + 1 >= branch.points.size()) continue;

                V tip = tendrilTip(b, idx, w);

                if (dist(seed.p, tip) < 26 * w.scale) {
                    seed.caught = true;
                    seed.caughtAge = 0;
                    seed.caughtBranch = b;
                    seed.caughtIndex = idx;
                    seed.velocity = {};

                    w.audio.play(2, 760, 0.04);
                    break;
                }
            }
        }
    }

    for (Seed& seed : seeds) {
        if (!seed.caught) continue;

        const Branch& branch = branches[seed.caughtBranch];
        V tip = tendrilTip(seed.caughtBranch, seed.caughtIndex, w);

        if (seed.caughtAge < 1.2) {
            // 短暂悬停在卷须尖端，冒一点子叶（画在 draw() 里）。
            seed.p = mix(seed.p, tip, follow(14, dt));
        } else {
            // 被卷须拉回去，缩回卷须根部后消失。
            V anchor = branch.points[seed.caughtIndex];
            double t = std::min(1.0, (seed.caughtAge - 1.2) / 0.8);

            seed.p = mix(tip, anchor, smooth(t));

            if (t >= 1.0) seed.life = 0;
        }
    }

    std::erase_if(seeds, [&](const Seed& seed) {
        return seed.life <= 0 ||
               (!seed.caught && seed.life < 2.5 &&
                dist(seed.p, root) < 20 * w.scale);
    });
}

void Plant::updateDrops(World& w, double dt) {
    dropClock -= dt;

    if (dropClock <= 0 && dragPart != 3 && drops.size() < 4) {
        dropClock = 6.5;

        int branchIndex = static_cast<int>(age) & 1;
        const Branch& branch = branches[branchIndex];

        size_t index = 4 +
            (static_cast<size_t>(age) %
             std::max<size_t>(1, (branch.points.size() - 6) / 2)) * 2;

        index = std::min(index, branch.points.size() - 2);

        Leaf leaf = leafGeometry(branchIndex, index, w);

        drops.push_back({
            leaf.base,
            {},
            branchIndex,
            index,
            0,
            14,
            false
        });
    }

    for (size_t i = 0; i < drops.size(); ++i) {
        Drop& drop = drops[i];

        drop.life -= dt;

        if (dragPart == 3 &&
            dragDrop == static_cast<std::ptrdiff_t>(i))
        {
            drop.p = w.mouse + offset;
            drop.falling = true;
            drop.velocity = {};
            continue;
        }

        if (!drop.falling) {
            Leaf leaf = leafGeometry(
                drop.branch, drop.leaf, w);

            drop.along += dt * 0.14;
            drop.p = mix(
                leaf.base, leaf.tip,
                std::min(1.0, drop.along));

            if (drop.along >= 1) {
                drop.falling = true;
                drop.velocity = {0, 5 * w.scale};
            }
        } else {
            drop.velocity.y += 120 * w.scale * dt;
            drop.p += drop.velocity * dt;

            for (int branchIndex = 0; branchIndex < 2; ++branchIndex) {
                const Branch& branch = branches[branchIndex];

                for (size_t leafIndex = 2;
                     leafIndex + 1 < branch.points.size();
                     leafIndex += 2)
                {
                    if (branchIndex == drop.branch &&
                        leafIndex == drop.leaf)
                        continue;

                    Leaf leaf = leafGeometry(
                        branchIndex, leafIndex, w);

                    if (drop.velocity.y > 0 &&
                        segmentDistance(drop.p, leaf.base, leaf.tip) < 5 * w.scale)
                    {
                        drop.branch = branchIndex;
                        drop.leaf = leafIndex;
                        drop.along = 0.35;
                        drop.falling = false;
                        drop.velocity = {};

                        branches[branchIndex].touch = 0.35;
                        branches[branchIndex].touchPosition =
                            static_cast<double>(leafIndex);

                        w.audio.play(2, 920, 0.045);
                        break;
                    }
                }

                if (!drop.falling) break;
            }
        }

        if (dist(drop.p, root) < 22 * w.scale)
            drop.life = 0;

        if (drop.p.y > root.y + 70 * w.scale)
            drop.life = 0;
    }

    // 拖动期间保持索引稳定。
    if (dragPart != 3) {
        std::erase_if(drops, [](const Drop& drop) {
            return drop.life <= 0;
        });
    }
}

void Plant::updateLeafRenewal(int branchIndex, World& w, double dt) {
    Branch& branch = branches[branchIndex];

    branch.renewalClock -= dt;

    if (branch.renewalClock <= 0 && branch.renewingLeaf < 0 &&
        branch.points.size() > 6)
    {
        branch.renewalClock = 12 + std::sin(age * 0.31 + branchIndex) * 6;

        size_t span = std::max<size_t>(1, (branch.points.size() - 4) / 2);
        size_t index = 2 + (static_cast<size_t>(age * 0.53) % span) * 2;

        if (index + 1 >= branch.points.size())
            index -= 2;

        branch.renewingLeaf = static_cast<int>(index);
        branch.renewalAge = 0;
    }

    if (branch.renewingLeaf >= 0) {
        branch.renewalAge += dt;

        if (branch.renewalAge > 2.5) {
            size_t index = static_cast<size_t>(branch.renewingLeaf);

            if (index + 1 < branch.points.size()) {
                Leaf leaf = leafGeometry(branchIndex, index, w);

                fallingLeaves.push_back({
                    leaf.tip,
                    V{std::sin(age) * 18 * w.scale, 12 * w.scale},
                    0,
                    3.5
                });
            }

            branch.renewingLeaf = -1;
        }
    }
}

void Plant::updateFallingLeaves(World& w, double dt) {
    for (FallingLeaf& leaf : fallingLeaves) {
        leaf.life -= dt;
        leaf.rotation += dt * 2.4;

        leaf.velocity.x += std::sin(age * 1.7 + leaf.rotation) * 14 * w.scale * dt;
        leaf.velocity.y += 24 * w.scale * dt;
        leaf.velocity *= std::exp(-0.6 * dt);

        leaf.p += leaf.velocity * dt;
    }

    std::erase_if(fallingLeaves, [](const FallingLeaf& leaf) {
        return leaf.life <= 0;
    });
}

} // namespace pets
