# Desktop Pets (gen3)

> Three small creatures that live on the edges of your windows, each with its own temperament.

![platform](https://img.shields.io/badge/platform-Windows-0a84ff)
![language](https://img.shields.io/badge/language-C%2B%2B20-00599C)
![license](https://img.shields.io/badge/license-MIT-green)

Read this in other languages: [中文](README.zh-CN.md)

---

### 🐛🪼🌿 The three characters

**🐛 Spider** patrols your window edges and stops to inspect them, and when it decides to actually do maintenance work it pulls out real tools — probing, clamping, fastening, spot-welding, a final brush pass — each move matching what it's actually doing. Whatever edge just moved gets checked first. Dragging it slowly versus flicking it fast gets different reactions, and it casts a faint contact shadow on whatever it's standing on. The right-click menu lets you adjust how often it bothers doing maintenance at all.

**🪼 Youxi** doesn't have a fixed body shape — its membrane and tendrils keep deforming based on whatever it's currently doing: speeding up, turning, being grabbed, being let go, each looking distinct, with every transition staying soft. It builds and tends its own nest. There's a gap in the middle of its body, and the gap turns to face your cursor as it gets close; passing through it gets a visible reaction. Every so often it doubles back to catch up with its own motion trail, or rides the draft from a window you just dragged past it before circling back.

**🌿 Plant**'s branches look for things to climb on their own, and if two windows happen to sit close enough, it'll actually bridge between them — pull the windows apart and the bridge slowly unwinds. Its flowers open and close on their own schedule and eventually release winged seeds, which its tendrils will try to catch as they drift by. Pick it up by the root to move it and the branches gather in and sway along; a stuck tendril can be grabbed directly and pulled loose on its own.

### 🏗️ Architecture

- **Rendering**: DirectComposition + Direct3D 11 + Direct2D for a transparent, click-through, always-on-top window. Only basic vector primitives (ellipses, polygons, lines) are used — no blur or shadow effects — and the window's actual bounds and hit-test region are recomputed every frame from the bounding box of whatever gets drawn.
- **Animation**: fully procedural — IK, FABRIK, and Verlet chains, no baked animation frames. A character's "state" is mostly a handful of parameters that spring/ease toward target values, so transitions between states are smooth by construction.
- **Desktop awareness**: a `Desktop` layer turns the edges of every other on-screen window into `Surface`s that characters can walk on, cling to, or bridge between, while also tracking window move/open/close events that feed directly into character reactions.
- **Dependencies**: plain C++20, nothing beyond Win32/D3D/D2D.

### 📦 Download & run

**Prebuilt download**

If the repo has a Release published, grab the zip from the Releases page, unzip it, and double-click `DesktopPetsGen3.exe` — no build tools needed. (No Release has been published yet; this is a placeholder for when one exists.)

**Build from source**

Requires Windows + MinGW-w64 (C++20-capable gcc) + CMake ≥ 3.24 + Ninja.

```bash
git clone <repo-url>
cd gen3
cmake -G Ninja -B build
cmake --build build
./build/DesktopPetsGen3.exe
```

Right-click the character to switch characters, resize, change the color palette, and (spider only) adjust how often it bothers doing maintenance.

### 📄 License

[MIT](LICENSE).

Still an actively evolving prototype; behavior and tuning will keep changing.
