#pragma once

#include "../core/math.hpp"
#include "../platform/settings.hpp"
#include "../audio/procedural_audio.hpp"
#include "../desktop/environment.hpp"
#include "../render/scene.hpp"

#include <vector>

namespace pets {

enum Command {
    Inspect = 200,
    Summon,
    ReturnHome,
    PlaceHome,
    Gather,
    Expand
};

struct World {
    Desktop desktop;
    Audio audio;

    V mouse{};
    V previousMouse{};

    double scale = 1;
    int palette = 0;
};

class Character {
public:
    virtual ~Character() = default;

    virtual const wchar_t* name() const = 0;

    virtual void initialize(World&, const Settings&) = 0;
    virtual void tick(World&, double dt) = 0;
    virtual void draw(Scene&, const World&) const = 0;

    virtual bool down(V point, World&) = 0;
    virtual void up(World&) = 0;

    virtual void command(int command, V point, World&) = 0;
    virtual void save(const Settings&) const = 0;
};

struct Spark {
    V p{}, velocity{};
    double life = 0;
};

inline void updateSparks(std::vector<Spark>& sparks, double dt) {
    for (Spark& p : sparks) {
        p.life -= dt;
        p.velocity.y += 90 * dt;
        p.p += p.velocity * dt;
    }

    std::erase_if(sparks, [](const Spark& p) { return p.life <= 0; });
}

} // namespace pets
