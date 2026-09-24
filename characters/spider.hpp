#pragma once

#include "character.hpp"
#include "../animation/solvers.hpp"

#include <array>
#include <vector>

namespace pets {

class Spider final : public Character {
public:
    const wchar_t* name() const override { return L"机械蜘蛛"; }

    void initialize(World& w, const Settings& settings) override;

    bool down(V p, World& w) override;
    void up(World& w) override;
    void command(int command, V point, World& w) override;
    void save(const Settings& settings) const override;

    void tick(World& w, double dt) override;
    void draw(Scene& scene, const World& w) const override;

private:
    struct Leg {
        V foot{}, from{}, to{};
        double progress = 1;
        bool stepping = false;
    };

    V body{}, offset{}, normal{0, -1};
    double angle = 0, topView = 0;

    std::array<Leg, 8> legs{};
    std::array<V, 2> tools{};

    Surface support{};
    double u = 0.5;
    double destination = 0.75;

    bool held = false, attached = false, working = false;
    bool tethered = false, hasJob = false;

    double age = 0, decision = 2, gait = 0;
    double workAge = 0, compression = 0, verticalSpeed = 0;
    double sparkClock = 0;

    V job{};
    std::vector<Spark> sparks;

    Surface tetherSurface{}, destinationSurface{};
    V tetherAnchor{}, tetherVelocity{};
    double tetherAnchorU = 0.5, destinationU = 0.5;

    double rideHeight(const World& w) const;
    V local(V p, const World& w) const;
    V hip(int i, const World& w) const;

    void translate(V delta);
    void shiftTools(V delta);

    void attach(const Surface& s, double parameter, World& w);
    bool tryCorner(World& w);
    const Surface* chooseLower(World& w) const;

    void beginTether(const Surface& destinationTarget, double parameter, World& w);
    void tickTether(World& w, double dt);
    void fall(World& w, double dt);

    void hangLegs(const World& w, double dt);
    void updateTools(World& w, double dt);
};

} // namespace pets
