#pragma once

#include "characters/character.hpp"
#include "characters/spider.hpp"
#include "characters/youxi.hpp"
#include "characters/plant.hpp"
#include "platform/settings.hpp"
#include "render/composition_renderer.hpp"

#include <array>
#include <memory>

namespace pets {

class Application {
public:
    Application();

    LRESULT handle(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

private:
    static constexpr UINT TrayMessage = WM_APP + 1;
    static constexpr UINT TimerId = 1;

    enum MenuId {
        ChooseSpider = 1001,
        ChooseYouxi = 1002,
        ChoosePlant = 1003,

        TogglePause = 1010,
        ToggleSound = 1011,
        ToggleFullscreen = 1012,

        SizeSmall = 1020,
        SizeNormal = 1021,
        SizeLarge = 1022,
        SizeExtra = 1023,

        PaletteOne = 1030,
        PaletteTwo = 1031,
        PaletteThree = 1032,

        Quit = 1099
    };

    HWND window{};

    Settings settings;
    World world;
    Renderer renderer;

    std::array<std::unique_ptr<Character>, 3> characters;

    size_t active = 0;

    bool initialized = false;
    bool paused = false;
    bool dragging = false;
    bool picking = false;

    bool hideFullscreen = true;
    bool currentlyHidden = false;

    int pickCommand = 0;

    UINT taskbarCreated = 0;

    LARGE_INTEGER frequency{};
    LARGE_INTEGER previousCounter{};

    double accumulator = 0;
    double desktopClock = 0;
    double saveClock = 0;

    Character& current();

    void initialize(HWND hwnd);
    void updateMouse();
    void tick();
    void render();
    bool foregroundFullscreen() const;

    void beginPick(int command);
    void cancelPick();
    void execute(int command);

    void showMenu();

    void addTray();
    void updateTrayTip();
    void removeTray() const;

    void save() const;
};

} // namespace pets
