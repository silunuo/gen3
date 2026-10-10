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
        ShortThread,
        Nest
    };

    enum class Posture {
        OpenRing, FoldedShuttle, Crescent, Sail, Pouch, ClosedPetal, Bloom
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
    bool nestTouch = false;

    double age = 0;
    double targetClock = 0;
    double sleepAge = 0;
    double fold = 0;
    double weave = 1;

    double nestWave = 0;
    double releaseBloom = 0;
    double throughPulse = 0;

    double contractPulse = 0;
    double diffusePulse = 0;
    double convergePulse = 0;

    Posture posture = Posture::OpenRing;
    double previousFacingAngle = 0;
    double turnRate = 0, crescentBias = 0;
    double compact = 0, sailBoost = 0, shuttleBoost = 0;

    double echoClock = 7;
    double trailClock = 0;

    V nestDownPoint{};
    double nestDownAge = -1;
    bool pendingNestGrab = false;
    double nestPluckClock = 9;

    bool fishing = false;
    V fishingAnchor{};
    double fishingTimer = 0, fishClock = 0;

    double renderStretch = 0, renderMouseNear = 0;

    bool chasingEcho = false;
    V chaseTarget{};
    double chaseGraceClock = 6;

    bool drafting = false;
    V draftHome{}, draftAnchor{};
    double draftTimer = 0;

    double holeApproach = 0, holeFaceBias = 0;
    bool mouseInHole = false;

    double threadPulse = 0;
    std::array<double, 3> nodeThreadDelay{-1, -1, -1};
    int threadCount = 0;
    double threadWindowClock = 0;
    bool inviting = false;
    double inviteClock = 0;

    std::vector<Spark> motes;

    static double membraneAngle(int index);

    void chooseTarget(World& w);
    void triggerBloom(World& w, double strength = 1.0);
    void triggerThread(World& w);
    void updateDisturbance(World& w, double dt);
    void updateEchoes(World& w, double dt);
    void drawNest(Scene& scene, const World& w) const;
};

} // namespace pets
