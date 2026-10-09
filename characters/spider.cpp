#include "spider.hpp"

#include <cmath>

namespace pets {

void Spider::initialize(World& w, const Settings& settings) {
    body = settings.position(L"Spider", w.desktop.home);

    if (!w.desktop.onScreen(body))
        body = w.desktop.home;

    if (const Surface* ground = w.desktop.groundAt(body.x)) {
        attach(*ground, ground->parameter({body.x, ground->a.y}), w);
    }

    for (int i = 0; i < 2; ++i)
        tools[i] = body;
}

bool Spider::down(V p, World& w) {
    if (dist(p, body) > 65 * w.scale) return false;

    held = true;
    offset = body - p;

    attached = false;
    tethered = false;
    working = false;
    verticalSpeed = 0;

    for (Leg& leg : legs) leg.stepping = false;

    w.audio.play(0, 430);
    return true;
}

void Spider::up(World& w) {
    if (!held) return;

    held = false;

    const Surface* surface = w.desktop.nearest(
        body - normal * rideHeight(w),
        75 * w.scale,
        false,
        130 * w.scale);

    if (surface)
        attach(*surface, surface->parameter(body), w);
}

void Spider::command(int command, V point, World& w) {
    if (command != Inspect) return;

    const Surface* surface = w.desktop.nearest(
        point, 220 * w.scale, false, 130 * w.scale);

    if (!surface) return;

    job = point;
    hasJob = true;

    if (attached && support.sameOwner(*surface)) {
        destination = std::clamp(
            surface->parameter(point), 0.12, 0.88);
    } else if (attached) {
        beginTether(*surface, surface->parameter(point), w);
    }
}

void Spider::save(const Settings& settings) const {
    settings.setPosition(L"Spider", body);
}

void Spider::tick(World& w, double dt) {
    age += dt;
    updateSparks(sparks, dt);

    if (held) {
        V old = body;
        body = mix(body, w.mouse + offset, follow(25, dt));

        shiftTools(body - old);
        angle += wrapAngle(-angle) * follow(5, dt);
        normal = mix(normal, {0, -1}, follow(5, dt));

        hangLegs(w, dt);
        updateTools(w, dt);
        return;
    }

    if (tethered) {
        tickTether(w, dt);
        hangLegs(w, dt);
        updateTools(w, dt);
        return;
    }

    if (attached) {
        const Surface* next = w.desktop.resolve(support);

        if (!next) {
            attached = false;
            working = false;
            verticalSpeed = 0;
        } else {
            V delta = next->at(u) - support.at(u);
            translate(delta);
            support = *next;
        }
    }

    if (!attached) {
        fall(w, dt);
        hangLegs(w, dt);
        updateTools(w, dt);
        return;
    }

    normal = unit(mix(
        normal, support.outward, follow(9, dt)));

    V tangent = support.tangent();
    double targetAngle = std::atan2(tangent.y, tangent.x);

    angle += wrapAngle(targetAngle - angle) * follow(8, dt);

    double targetView =
        std::abs(tangent.y) > 0.5 ? 1.0 : 0.0;

    topView += (targetView - topView) * follow(6, dt);
    compression *= std::exp(-12 * dt);

    if (hasJob && segmentDistance(job, support.a, support.b) < 180 * w.scale) {
        destination = std::clamp(
            support.parameter(job), 0.12, 0.88);
    }

    decision -= dt;

    if (decision <= 0 && !hasJob) {
        decision = 4.5 + std::sin(age * 0.7) * 1.2;
        destination = 0.15 + (0.5 + 0.5 * std::sin(age * 0.83)) * 0.7;

        double roll = std::sin(age * 0.29);

        if (roll > 0.55) {
            const Surface* lower = chooseLower(w);

            if (lower) {
                beginTether(*lower, 0.5, w);
                return;
            }
        } else if (roll < -0.55) {
            const Surface* higher = chooseHigher(w);

            if (higher) {
                beginTether(*higher, 0.5, w);
                return;
            }
        }
    }

    double delta = destination - u;
    bool arrived = std::abs(delta) * support.length() < 5 * w.scale;

    working = arrived;

    if (working) {
        workAge += dt;

        if (workAge > 4.8) {
            working = false;
            workAge = 0;
            hasJob = false;

            if (!tryCorner(w))
                destination = destination < 0.5 ? 0.84 : 0.16;
        }
    } else {
        double speed = 74 * w.scale;
        double step = speed * dt / std::max(1.0, support.length());

        u += std::clamp(delta, -step, step);
    }

    double endMargin = std::min(
        0.35, 76 * w.scale / support.length());

    u = std::clamp(u, endMargin, 1 - endMargin);

    V targetBody =
        support.at(u) + normal * rideHeight(w);

    body = mix(body, targetBody, follow(18, dt));

    gait += dt * (working ? 1.8 : 4.1);
    int group = static_cast<int>(gait) & 1;

    for (int i = 0; i < 8; ++i) {
        Leg& leg = legs[i];

        double spread = (i - 3.5) * 24 * w.scale;
        double footU = u + spread / support.length();

        footU = std::clamp(footU, 0.01, 0.99);

        V desired =
            support.at(footU) +
            normal * ((i & 1) ? 2 : -2) * topView * w.scale;

        bool tooFar =
            dist(hip(i, w), leg.foot) > 128 * w.scale;

        bool takeStep =
            dist(leg.foot, desired) > 27 * w.scale &&
            (i & 1) == group;

        if (!leg.stepping && (takeStep || tooFar)) {
            leg.from = leg.foot;
            leg.to = desired;
            leg.progress = 0;
            leg.stepping = true;
        }

        if (leg.stepping) {
            leg.progress = std::min(1.0, leg.progress + dt / 0.25);

            leg.foot = mix(
                leg.from, leg.to, smooth(leg.progress));

            leg.foot += normal *
                (std::sin(leg.progress * PI) * 20 * w.scale);

            if (leg.progress >= 1) {
                leg.foot = leg.to;
                leg.stepping = false;
            }
        }
    }

    updateTools(w, dt);
}

void Spider::draw(Scene& scene, const World& w) const {
    double s = w.scale;

    if (attached && !held) {
        contactShadow(scene, support, support.at(u), 58 * s, 16 * s, 0.85);
    }

    if (tethered) {
        scene.disk(tetherAnchor, 5 * s, color(0xB3B9B5));
        scene.line(tetherAnchor, body, color(0x8B969E, 0.9f), 1.4 * s);
    }

    for (int layer = 0; layer < 2; ++layer) {
        for (int i = 0; i < 8; ++i) {
            if ((i & 1) != layer) continue;

            V root = hip(i, w);
            double bend = i < 4 ? 1 : -1;

            TwoBone pose = ik2(
                root, legs[i].foot, 61 * s, 72 * s, bend);

            Color metal = color(
                layer == 0 ? 0x657B8C : 0xB8C7D0);

            scene.line(root, pose.joint, color(0x283640), 8 * s);
            scene.line(root, pose.joint, metal, 5 * s);

            scene.line(pose.joint, pose.end, color(0x27343E), 6 * s);
            scene.line(pose.joint, pose.end, metal, 3.5 * s);

            scene.disk(pose.joint, 5 * s, color(0xC89B59));
            scene.disk(pose.joint, 2 * s, color(0x4E5457));

            V toe = rotate({6 * s, 0}, angle);

            scene.line(pose.end - toe, pose.end + toe,
                       color(0xD2DDE1), 3 * s);
        }
    }

    std::vector<V> hull;

    for (int i = 0; i < 32; ++i) {
        double a = TAU * i / 32;

        hull.push_back(body + rotate({
            std::cos(a) * 51 * s,
            std::sin(a) * (23 + topView * 8) * s
        }, angle));
    }

    Color top = color(
        w.palette == 1 ? 0xBAA57C :
        w.palette == 2 ? 0x9AA7AC : 0xA6BDC8);

    scene.poly(hull, top, color(0x344956));

    V seamA = local({-29, -5}, w);
    V seamB = local({20, -5}, w);
    scene.line(seamA, seamB, color(0xD2D8D6, 0.65f), 1.3 * s);

    V sensor = local({29, -10}, w);
    scene.ellipse(sensor, {17 * s, 12 * s},
                  color(0x263947), color(0x14222D));

    V look = unit(w.mouse - sensor);
    scene.disk(sensor + look * (8 * s), 4.5 * s, color(0x79E9D5));

    for (int i = 0; i < 3; ++i)
        scene.disk(local({-26 + i * 8.0, 9}, w),
                   1.7 * s, color(i == 0 ? 0xE8BE6C : 0x728D98));

    for (int i = 0; i < 2; ++i) {
        V root = local({17 + i * 11.0, 12}, w);

        TwoBone tool = ik2(
            root, tools[i], 29 * s, 34 * s, i == 0 ? 1 : -1);

        scene.line(root, tool.joint, color(0xC9C5A5), 3 * s);
        scene.line(tool.joint, tool.end, color(0xCFD6CE), 2 * s);

        int stage = static_cast<int>(workAge) % 5;

        if (working && stage == 0) {
            scene.ring(tool.end, 7 * s, 4 * s,
                       color(0x83DBC8, 0.65f), s);
        } else if (working && stage == 1) {
            V jaw = rotate({4 * s, 0}, age * 3);
            scene.line(tool.end - jaw, tool.end + jaw,
                       color(0xE0BF75), 2 * s);
        } else if (working && stage == 3) {
            scene.line(tool.end, tool.end + normal * (6 * s),
                       color(0xDFD3B0), 4 * s);
        } else {
            scene.disk(tool.end, 2.8 * s, color(0xE5B66B));
        }
    }

    if (working && workAge > 3.8) {
        V a = tools[0], b = tools[1];

        for (int i = 1; i <= 12; ++i) {
            double t0 = (i - 1) / 12.0;
            double t1 = i / 12.0;

            V p0 = mix(a, b, t0) + normal *
                (std::sin(t0 * PI) * 12 * s);

            V p1 = mix(a, b, t1) + normal *
                (std::sin(t1 * PI) * 12 * s);

            scene.line(p0, p1, color(0xB6AC80), 1.2 * s);
        }
    }

    for (const Spark& p : sparks)
        scene.line(p.p, p.p - p.velocity * 0.025,
                   color(0xFFD88A,
                         static_cast<float>(std::min(1.0, p.life * 5))),
                   1.3 * s);
}

double Spider::rideHeight(const World& w) const {
    return (66 - topView * 34) * w.scale;
}

V Spider::local(V p, const World& w) const {
    return body + rotate(p * w.scale, angle) -
           normal * compression;
}

V Spider::hip(int i, const World& w) const {
    double side = (i & 1) ? 1 : -1;

    return local({
        (i - 3.5) * 9,
        4 + side * topView * 16
    }, w);
}

void Spider::translate(V delta) {
    body += delta;

    for (Leg& leg : legs) {
        leg.foot += delta;
        leg.from += delta;
        leg.to += delta;
    }

    shiftTools(delta);
}

void Spider::shiftTools(V delta) {
    for (V& tool : tools) tool += delta;
}

void Spider::attach(const Surface& s, double parameter, World& w) {
    support = s;
    u = std::clamp(parameter, 0.12, 0.88);

    attached = true;
    tethered = false;
    verticalSpeed = 0;

    normal = support.outward;
    angle = std::atan2(s.tangent().y, s.tangent().x);
    topView = std::abs(s.tangent().y) > 0.5 ? 1 : 0;

    V target = support.at(u) + normal * rideHeight(w);
    translate(target - body);

    compression = 6 * w.scale;
    destination = u < 0.5 ? 0.8 : 0.2;

    for (int i = 0; i < 8; ++i) {
        double footU = u +
            (i - 3.5) * 24 * w.scale / support.length();

        legs[i].foot = support.at(footU);
        legs[i].stepping = false;
    }

    w.audio.play(0, 260, 0.08);
}

bool Spider::tryCorner(World& w) {
    if (support.ground) return false;

    V corner = u < 0.5 ? support.a : support.b;

    for (const Surface& next : w.desktop.surfaces) {
        if (next.owner != support.owner ||
            next.edge == support.edge ||
            next.ground)
            continue;

        if (dist(next.a, corner) > 4 &&
            dist(next.b, corner) > 4)
            continue;

        // 沿接近的端点换面，足端保持原位置后逐足重新落点。
        support = next;
        u = dist(next.a, corner) < dist(next.b, corner) ? 0.14 : 0.86;
        destination = u < 0.5 ? 0.8 : 0.2;
        return true;
    }

    return false;
}

const Surface* Spider::chooseLower(World& w) const {
    const Surface* result = nullptr;
    double best = 1e30;

    for (const Surface& s : w.desktop.surfaces) {
        if (s.edge != 0 && !s.ground) continue;
        if (s.sameOwner(support)) continue;

        V landing = closestOnSegment(body, s.a, s.b);

        if (landing.y < body.y + 90 * w.scale ||
            landing.y > body.y + 650 * w.scale ||
            std::abs(landing.x - body.x) > 300 * w.scale ||
            s.length() < 190 * w.scale)
            continue;

        double score = dist(body, landing);

        if (score < best) {
            best = score;
            result = &s;
        }
    }

    return result;
}

const Surface* Spider::chooseHigher(World& w) const {
    const Surface* result = nullptr;
    double best = 1e30;

    for (const Surface& s : w.desktop.surfaces) {
        if (s.edge != 0 && !s.ground) continue;
        if (s.sameOwner(support)) continue;

        V landing = closestOnSegment(body, s.a, s.b);

        if (landing.y > body.y - 90 * w.scale ||
            landing.y < body.y - 650 * w.scale ||
            std::abs(landing.x - body.x) > 300 * w.scale ||
            s.length() < 190 * w.scale)
            continue;

        double score = dist(body, landing);

        if (score < best) {
            best = score;
            result = &s;
        }
    }

    return result;
}

void Spider::beginTether(const Surface& destinationTarget, double parameter, World& w) {
    tetherSurface = support;
    tetherAnchorU = u;
    tetherAnchor = support.at(u);

    destinationSurface = destinationTarget;
    destinationU = std::clamp(parameter, 0.15, 0.85);

    tetherVelocity = {};
    tethered = true;
    attached = false;
    working = false;

    w.audio.play(0, 520, 0.08);
}

void Spider::tickTether(World& w, double dt) {
    const Surface* anchor = w.desktop.resolve(tetherSurface);
    const Surface* destinationTarget =
        w.desktop.resolve(destinationSurface);

    if (!anchor || !destinationTarget) {
        tethered = false;
        verticalSpeed = 0;
        return;
    }

    tetherSurface = *anchor;
    destinationSurface = *destinationTarget;

    tetherAnchor = anchor->at(tetherAnchorU);

    V target = destinationTarget->at(destinationU) +
               destinationTarget->outward * (66 * w.scale);

    spring(body, tetherVelocity, target, 5.0, 4.8, dt);

    angle += wrapAngle(0 - angle) * follow(4, dt);
    normal = unit(mix(normal, {0, -1}, follow(4, dt)));
    topView += (0 - topView) * follow(4, dt);

    if (dist(body, target) < 10 * w.scale)
        attach(*destinationTarget, destinationU, w);
}

void Spider::fall(World& w, double dt) {
    double previous = body.y + 66 * w.scale;

    verticalSpeed += 980 * w.scale * dt;
    body.y += verticalSpeed * dt;

    double current = body.y + 66 * w.scale;

    const Surface* landing = nullptr;
    double bestY = 1e30;

    for (const Surface& s : w.desktop.surfaces) {
        if (s.edge != 0 && !s.ground) continue;
        if (body.x < s.a.x + 80 * w.scale ||
            body.x > s.b.x - 80 * w.scale)
            continue;

        bool crossed = previous <= s.a.y && current >= s.a.y;
        bool belowGround = s.ground && current >= s.a.y;

        if ((crossed || belowGround) && s.a.y < bestY) {
            bestY = s.a.y;
            landing = &s;
        }
    }

    if (landing)
        attach(*landing, landing->parameter(body), w);
}

void Spider::hangLegs(const World& w, double dt) {
    for (int i = 0; i < 8; ++i) {
        V hanging = body + rotate(V{
            (i - 3.5) * 15 * w.scale,
            91 * w.scale
        }, angle);

        legs[i].foot = mix(
            legs[i].foot, hanging, follow(10, dt));

        legs[i].stepping = false;
    }
}

void Spider::updateTools(World& w, double dt) {
    for (int i = 0; i < 2; ++i) {
        V desired = local({28 + i * 10.0, 18}, w);

        if (working) {
            desired = local({
                53 + i * 7.0 + std::sin(age * 5 + i) * 3,
                42 + std::cos(age * 4 + i) * 3
            }, w);
        }

        tools[i] = mix(tools[i], desired, follow(12, dt));
    }

    if (working && workAge >= 2 && workAge < 3) {
        sparkClock -= dt;

        if (sparkClock <= 0) {
            sparkClock = 0.12;

            for (int i = 0; i < 3; ++i) {
                sparks.push_back({
                    tools[0],
                    rotate({55 + i * 25.0, -80}, age * 3 + i),
                    0.18 + i * 0.04
                });
            }

            w.audio.play(0, 840, 0.035);
        }
    }
}

} // namespace pets
