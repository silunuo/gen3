#pragma once

#include "character.hpp"
#include "../animation/solvers.hpp"

#include <array>
#include <vector>

namespace pets {

class Youxi final : public Character {
public:
    Youxi() : longThread(24, 8), shortThread(15, 6) {}

    const wchar_t* name() const override { return L"游隙"; }

    void initialize(World& w, const Settings& settings) override;

    bool down(V p, World& w) override;
    void up(World& w) override;
    void command(int command, V p, World& w) override;
    void save(const Settings& settings) const override;

    void tick(World& w, double dt) override;
    void draw(Scene& scene, const World& w) const override;

private:
    static constexpr int membraneCount = 40;

    enum class Drag {
        None,
        Body,
        Thread,
        Nest
    };

    struct Echo {
        V p{}, velocity{};
        double radius = 25;
        double life = 4;
    };

    Drag drag = Drag::None;

    V center{}, velocity{}, target{};
    V offset{}, facing{1, 0};

    V nest{};
    V disturbance{};

    std::array<V, membraneCount> membrane{};
    std::array<V, membraneCount> membraneVelocity{};

    std::array<V, 3> nodes{};
    std::array<V, 3> nodeVelocity{};

    Filament longThread;
    Filament shortThread;

    std::vector<Echo> echoes;
    std::vector<Spark> trail;

    Surface observedSurface{};
    bool observing = false;

    bool returning = false;
    bool resting = false;
    bool wasInside = false;

    double age = 0;
    double targetClock = 0;
    double sleepAge = 0;
    double fold = 0;
    double weave = 1;

    double nestWave = 0;
    double releaseBloom = 0;
    double throughPulse = 0;

    double echoClock = 7;
    double trailClock = 0;

    static double membraneAngle(int index);

    void chooseTarget(World& w);
    void updateDisturbance(World& w, double dt);
    void updateEchoes(World& w, double dt);
    void drawNest(Scene& scene, const World& w) const;
};

} // namespace pets
