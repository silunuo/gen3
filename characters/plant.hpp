#pragma once

#include "character.hpp"
#include "../animation/solvers.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace pets {

class Plant final : public Character {
public:
    const wchar_t* name() const override { return L"栖枝"; }

    void initialize(World& w, const Settings& settings) override;

    bool down(V p, World& w) override;
    void up(World& w) override;
    void command(int command, V, World&) override;
    void save(const Settings& settings) const override;

    void tick(World& w, double dt) override;
    void draw(Scene& scene, const World& w) const override;

private:
    struct Branch {
        std::vector<V> points;

        V tip{}, tipVelocity{};
        V freeTarget{};

        Surface support{};
        double supportU = 0.5;

        bool attached = false;
        bool releasedSeeds = false;
        bool bridging = false;
        double release = 0;

        double attachBlend = 0;
        double freeTargetLife = 0;
        double phase = 0;
        double growth = 1;

        double touch = 0;
        double touchPosition = 0;

        int renewingLeaf = -1;
        double renewalAge = 0;
        double renewalClock = 14;
    };

    struct Leaf {
        V base{}, tip{}, upper{}, lower{};
    };

    struct FallingLeaf {
        V p{}, velocity{};
        double rotation = 0;
        double life = 3;
    };

    struct Drop {
        V p{}, velocity{};
        int branch = 0;
        size_t leaf = 2;
        double along = 0;
        double life = 10;
        bool falling = false;
    };

    struct Seed {
        V p{}, velocity{};
        double rotation = 0;
        double life = 8;

        bool caught = false;
        double caughtAge = 0;
        int caughtBranch = 0;
        size_t caughtIndex = 0;
    };

    V root{}, offset{};
    Surface rootSupport{};
    double rootU = 0.5;
    double rootVelocity = 0;

    bool rootAttached = false;

    std::array<Branch, 2> branches;
    std::vector<Drop> drops;
    std::vector<Seed> seeds;
    std::vector<FallingLeaf> fallingLeaves;

    int dragPart = -1;
    std::ptrdiff_t dragDrop = -1;

    double age = 0;
    double spread = 1;
    double desiredSpread = 1;
    double sleep = 0;

    double anchorClock = 2;
    double dropClock = 4;

    double segmentLength(const Branch& branch, const World& w) const;

    void translate(V delta);
    void updateRoot(World& w, double dt);
    void seekAnchors(World& w);

    Leaf leafGeometry(int branchIndex, size_t index, const World& w) const;

    V tendrilPoint(
        int branchIndex, size_t index, double u, const World& w) const;
    V tendrilTip(int branchIndex, size_t index, const World& w) const;

    void drawTendril(
        Scene& scene, int branchIndex, size_t index, const World& w) const;

    void drawFlower(
        Scene& scene, const Branch& branch, int branchIndex,
        const World& w) const;

    void releaseSeeds(int branchIndex, World& w);
    void updateSeeds(World& w, double dt);
    void updateDrops(World& w, double dt);
    void updateLeafRenewal(int branchIndex, World& w, double dt);
    void updateFallingLeaves(World& w, double dt);
};

} // namespace pets
