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

    // 轻点和按住拖拽要分开处理：先记录按下位置，真正判定留到 tick()，
    // 这样快速松开可以只触发"轻微一让"而不是把蜘蛛整个薅起来。
    downPoint = p;
    downAge = age;
    pendingGrab = true;

    return true;
}

void Spider::up(World& w) {
    if (pendingGrab) {
        pendingGrab = false;
        tapReact = 0.4;
        compression = 10 * w.scale;
        return;
    }

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

    if (!attached) return;

    working = false;
    patrol = Patrol::Approaching;

    if (support.sameOwner(*surface)) {
        patrolTargetU = std::clamp(surface->parameter(point), 0.12, 0.88);
        destination = patrolTargetU;
    } else {
        patrolTargetU = std::clamp(surface->parameter(point), 0.15, 0.85);
        beginTether(*surface, patrolTargetU, w);
    }
}

void Spider::save(const Settings& settings) const {
    settings.setPosition(L"Spider", body);
}

void Spider::tick(World& w, double dt) {
    age += dt;
    updateSparks(sparks, dt);

    if (tapReact > 0) tapReact -= dt;

    if (pendingGrab) {
        bool promote =
            dist(w.mouse, downPoint) > 18 * w.scale ||
            (age - downAge) > 0.18;

        if (promote) {
            pendingGrab = false;

            held = true;
            offset = body - w.mouse;

            attached = false;
            tethered = false;
            working = false;
            verticalSpeed = 0;

            for (Leg& leg : legs) leg.stepping = false;

            w.audio.play(0, 430);
        }
    }

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

            // 慢拖几乎看不出来，快拖会让身体明显向运动反方向后仰——
            // 不用另外判断"慢/快"的阈值，两种姿态是同一条缓动曲线的两端。
            V dragVelocity = delta / dt;
            V targetLean = limit(dragVelocity * -0.012, 22 * w.scale);
            dragLean = mix(dragLean, targetLean, follow(8, dt));

            // 表面被上层窗口新裁短到不安全的程度，当作"快被挡住了"，
            // 不等真走到裁短后的边缘掉下去，直接打断当前阶段重新选目标。
            if (next->length() < 150 * w.scale ||
                next->length() < support.length() * 0.5)
            {
                patrol = Patrol::Observing;
                patrolTimer = 0;
                working = false;
            }

            support = *next;
        }
    } else {
        dragLean = {};
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

    updatePatrol(w, dt);

    if (tethered) return;

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
    } else if (working && task == WorkTask::TetherCheck) {
        // 刚甩钩过来，回头看一眼那条线——复用吊索的画法，淡一些。
        scene.disk(tetherAnchor, 4 * s, color(0xB3B9B5, 0.8f));
        scene.line(tetherAnchor, body, color(0x8B969E, 0.6f), 1.1 * s);
    }

    for (int layer = 0; layer < 2; ++layer) {
        for (int i = 0; i < 8; ++i) {
            if ((i & 1) != layer) continue;

            V root = hip(i, w);

            // 膝盖永远弹向 normal（朝外），不按腿编号写死方向——
            // 否则某些朝向下固定的弯曲符号会让膝盖弹到支撑面内侧去。
            V axis = unit(legs[i].foot - root);
            double bend = dot(perp(axis), normal) >= 0 ? 1.0 : -1.0;

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

    auto drawTool = [&](int i) {
        V root = local({17 + i * 11.0, 12}, w);

        TwoBone tool = ik2(
            root, tools[i], 29 * s, 34 * s, i == 0 ? 1 : -1);

        scene.line(root, tool.joint, color(0xC9C5A5), 3 * s);
        scene.line(tool.joint, tool.end, color(0xCFD6CE), 2 * s);

        // 五种工具各自的外观：探测头、微型夹具、紧固头、点焊头、小刷头。
        auto drawProbe = [&] {
            scene.ring(tool.end, 7 * s, 4 * s,
                       color(0x83DBC8, 0.65f), s);
            scene.disk(tool.end, 1.6 * s, color(0xBFF2E6));
        };

        auto drawClamp = [&] {
            double spread = 0.35 + std::sin(age * 6) * 0.12;
            V jawA = rotate(V{5 * s, 0}, spread);
            V jawB = rotate(V{5 * s, 0}, -spread);

            scene.line(tool.end, tool.end + jawA, color(0xDCE3B8), 1.8 * s);
            scene.line(tool.end, tool.end + jawB, color(0xDCE3B8), 1.8 * s);
            scene.disk(tool.end, 1.6 * s, color(0xB9C98A));
        };

        auto drawFastener = [&] {
            V arm = rotate(V{4 * s, 0}, age * 9);
            scene.line(tool.end - arm, tool.end + arm,
                       color(0xE0BF75), 2 * s);
            scene.disk(tool.end, 1.8 * s, color(0xE0BF75));
        };

        auto drawWelder = [&] {
            scene.disk(tool.end, 2.6 * s, color(0xE58B5B));
            scene.disk(tool.end, 1.1 * s, color(0xFFE2B0));
        };

        auto drawBrush = [&] {
            double sweep = std::sin(age * 10) * 0.5;

            for (int b = -2; b <= 2; ++b) {
                V bristle = rotate(V{0, 6 * s}, sweep + b * 0.18);
                scene.line(tool.end, tool.end + bristle,
                           color(0xD9D0A8, 0.8f), 1.1 * s);
            }
        };

        if (working && task == WorkTask::EdgePatrol) {
            drawProbe();
        } else if (working && task == WorkTask::Fasten) {
            drawFastener();
        } else if (working && task == WorkTask::SpotWeld) {
            drawWelder();
        } else if (working && task == WorkTask::Wiring) {
            double t = workAge / 4.2;

            double extend =
                t < 0.35 ? t / 0.35 :
                t < 0.7  ? 1.0 :
                           std::max(0.0, 1.0 - (t - 0.7) / 0.3);

            V dir = support.tangent() * (i == 0 ? 1.0 : -1.0);
            V wireEnd = tool.end + dir * (26 * s * extend);

            scene.line(tool.end, wireEnd, color(0xC7B98A, 0.85f), 1.6 * s);
            drawClamp();
        } else if (working && task == WorkTask::ToolCheck) {
            // 工具自检：五种工具依次展开检查，最后用小刷头擦拭收尾。
            int phase = static_cast<int>(
                std::fmod(workAge, 3.0) / 0.6) % 5;

            switch (phase) {
                case 0: drawProbe(); break;
                case 1: drawClamp(); break;
                case 2: drawFastener(); break;
                case 3: drawWelder(); break;
                default: drawBrush(); break;
            }
        } else {
            scene.disk(tool.end, 2.8 * s, color(0xE5B66B));
        }
    };

    // 后臂先画，被身体外壳盖住根部，制造前后层级；前臂最后画，始终完整可见。
    drawTool(1);

    std::vector<V> hull;

    for (int i = 0; i < 32; ++i) {
        double a = TAU * i / 32;

        hull.push_back(body + dragLean + rotate({
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

    V look;

    if (tapReact > 0) {
        look = unit(downPoint - sensor);
    } else if (patrol == Patrol::Resting && patrolTimer < 0.3) {
        // 脱离前整理：出发前快速左右扫一眼。
        look = rotate(support.tangent(), std::sin(age * 10) * 0.9);
    } else if (patrol == Patrol::Inspecting || patrol == Patrol::Working) {
        look = support.tangent() * (destination >= u ? 1.0 : -1.0);
    } else {
        look = unit(w.mouse - sensor);
    }

    scene.disk(sensor + look * (8 * s), 4.5 * s, color(0x79E9D5));

    for (int i = 0; i < 3; ++i)
        scene.disk(local({-26 + i * 8.0, 9}, w),
                   1.7 * s, color(i == 0 ? 0xE8BE6C : 0x728D98));

    drawTool(0);

    if (working && task == WorkTask::SpotWeld && workAge > 0.8) {
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
    return body + dragLean + rotate(p * w.scale, angle) -
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

        // 沿接近的端点换面。新旧表面朝向往往相差很大（比如顶边转侧边），
        // 腿如果还留在旧表面上逐足补步，会出现身体已经贴到新面、
        // 腿却伸向旧面的割裂感，所以换面时直接把所有足端落到新表面上。
        support = next;
        u = dist(next.a, corner) < dist(next.b, corner) ? 0.14 : 0.86;
        destination = u < 0.5 ? 0.8 : 0.2;

        for (int i = 0; i < 8; ++i) {
            double footU = u +
                (i - 3.5) * 24 * w.scale / support.length();

            legs[i].foot = support.at(footU);
            legs[i].stepping = false;
        }

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

void Spider::updatePatrol(World& w, double dt) {
    switch (patrol) {
    case Patrol::Observing: {
        if (!hasJob) {
            patrolTimer -= dt;
            if (patrolTimer > 0) break;

            patrolTimer = 14 + std::sin(age * 0.23) * 6;
        }

        Surface target;
        double targetU = 0.5;
        bool priority = false;

        if (!choosePatrolTarget(w, target, targetU, priority)) break;

        patrolTargetU = targetU;
        patrolPriority = priority;

        if (support.sameOwner(target)) {
            destination = patrolTargetU;
            viaTether = false;
        } else {
            beginTether(target, patrolTargetU, w);
            viaTether = true;
        }

        patrol = Patrol::Approaching;
        break;
    }

    case Patrol::Approaching: {
        if (tethered) break;

        // attach() 落地后会把 destination 重置到表面另一端（通用探索的
        // 默认去向），这里要覆盖回巡逻真正瞄准的那个点，否则甩钩落地
        // 后会立刻又往别处走，永远不会真正"到达"去检查。
        destination = patrolTargetU;

        double delta = destination - u;
        bool arrived = std::abs(delta) * support.length() < 5 * w.scale;

        if (arrived) {
            patrol = Patrol::Inspecting;
            patrolTimer = 0.6 + std::sin(age * 1.3) * 0.2;
        } else {
            double speed = 74 * w.scale;
            double step = speed * dt / std::max(1.0, support.length());

            u += std::clamp(delta, -step, step);
        }

        break;
    }

    case Patrol::Inspecting: {
        patrolTimer -= dt;
        if (patrolTimer > 0) break;

        bool doWork = patrolPriority || viaTether || std::sin(age * 1.7) > 0.3;

        if (doWork) {
            task = viaTether ? WorkTask::TetherCheck : chooseTask();
            workAge = 0;
            working = true;
            patrol = Patrol::Working;
        } else {
            patrolTimer = 0.5;
            patrol = Patrol::Glancing;
        }

        break;
    }

    case Patrol::Working: {
        workAge += dt;

        double duration =
            task == WorkTask::EdgePatrol  ? 2.5 :
            task == WorkTask::ToolCheck   ? 3.0 :
            task == WorkTask::TetherCheck ? 3.0 :
            task == WorkTask::Fasten      ? 4.0 :
            task == WorkTask::Wiring      ? 4.2 : 4.5;

        if (workAge > duration) {
            working = false;
            workAge = 0;
            hasJob = false;
            patrolPriority = false;
            viaTether = false;

            patrolTimer = 3.0 + std::sin(age * 0.9) * 1.5;
            patrol = Patrol::Resting;
        }

        break;
    }

    case Patrol::Glancing: {
        patrolTimer -= dt;

        if (patrolTimer <= 0) {
            hasJob = false;
            patrolPriority = false;

            patrolTimer = 2.0;
            patrol = Patrol::Resting;
        }

        break;
    }

    case Patrol::Resting: {
        patrolTimer -= dt;

        if (patrolTimer <= 0) {
            if (!tryCorner(w))
                destination = destination < 0.5 ? 0.84 : 0.16;

            patrol = Patrol::Observing;
            patrolTimer = 0.3;
        }

        break;
    }
    }
}

bool Spider::choosePatrolTarget(
    World& w, Surface& outSurface, double& outU, bool& outPriority) const
{
    if (hasJob) {
        const Surface* surface = w.desktop.nearest(
            job, 240 * w.scale, false, 100 * w.scale);

        if (surface) {
            outSurface = *surface;
            outU = std::clamp(surface->parameter(job), 0.12, 0.88);
            outPriority = true;
            return true;
        }
    }

    struct Candidate { const Surface* surface; double weight; };
    std::vector<Candidate> candidates;

    for (const Surface& s : w.desktop.surfaces) {
        if (s.length() < 60 * w.scale) continue;

        double weight = 1.0;

        if (s.edge == 0) weight += 1.0;
        if (s.ground) weight += 1.5;
        if (w.desktop.recentlyMoved(s.owner, 8.0)) weight += 4.0;

        bool nearScreenEdge =
            std::abs(s.a.x - w.desktop.virtualRect.left) < 4 ||
            std::abs(s.b.x - w.desktop.virtualRect.right) < 4 ||
            std::abs(s.a.y - w.desktop.virtualRect.top) < 4 ||
            std::abs(s.b.y - w.desktop.virtualRect.bottom) < 4;

        if (nearScreenEdge) weight += 1.0;

        candidates.push_back({&s, weight});
    }

    if (const Surface* lower = chooseLower(w))
        candidates.push_back({lower, 2.5});

    if (const Surface* higher = chooseHigher(w))
        candidates.push_back({higher, 2.0});

    if (candidates.empty()) return false;

    double total = 0;
    for (const Candidate& c : candidates) total += c.weight;

    double pick = std::fmod(age * 37.0, total);
    double accum = 0;
    const Surface* chosen = candidates.back().surface;

    for (const Candidate& c : candidates) {
        accum += c.weight;

        if (pick <= accum) {
            chosen = c.surface;
            break;
        }
    }

    outSurface = *chosen;
    outU = 0.15 + (0.5 + 0.5 * std::sin(age * 0.83)) * 0.7;
    outPriority = w.desktop.recentlyMoved(chosen->owner, 8.0);

    return true;
}

WorkTask Spider::chooseTask() const {
    if (patrolPriority)
        return std::sin(age * 2.3) > 0 ? WorkTask::Fasten : WorkTask::SpotWeld;

    double roll = std::fmod(age * 11.0, 5.0);

    if (roll < 1.0) return WorkTask::EdgePatrol;
    if (roll < 2.0) return WorkTask::Fasten;
    if (roll < 3.0) return WorkTask::SpotWeld;
    if (roll < 4.0) return WorkTask::ToolCheck;
    return WorkTask::Wiring;
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

        if (working && task == WorkTask::ToolCheck) {
            // 两只工具臂轮流伸展再收回，错开一点相位，看起来像在依次自检。
            double phase = std::fmod(workAge - i * 0.8 + 3.0, 1.5);

            desired = phase < 0.75
                ? local({70, -10.0 + i * 50}, w)
                : local({28 + i * 10.0, 18}, w);
        } else if (working) {
            desired = local({
                53 + i * 7.0 + std::sin(age * 5 + i) * 3,
                42 + std::cos(age * 4 + i) * 3
            }, w);
        }

        tools[i] = mix(tools[i], desired, follow(12, dt));
    }

    if (working && task == WorkTask::SpotWeld && workAge >= 1.2 && workAge < 2.2) {
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
