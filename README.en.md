# Desktop Pets (gen3)

> A desktop pet built to test procedural IK generation.

![platform](https://img.shields.io/badge/platform-Windows-0a84ff)
![language](https://img.shields.io/badge/language-C%2B%2B20-00599C)
![license](https://img.shields.io/badge/license-MIT-green)

<p align="center">
  <img src="monika.png" width="180" alt="the project mascot giving you a side-eye">
  <br>
  <sub>The project mascot, side-eyeing you to check if you actually read the docs.</sub>
</p>

中文：[中文](README.md)

---

### 🐛🪼🌿 The three characters

**🐛 Spider** patrols your window edges and stops to inspect them; the right-click menu lets you adjust how often it bothers "working."

**🪼 Youxi** doesn't have a fixed body shape — its membrane and tendrils keep deforming based on whatever it's currently doing. A creature from a dream.

**🌿 Plant** is a plant.

### 🏗️ Architecture

- **Rendering**: DirectComposition + Direct3D 11 + Direct2D for a transparent, click-through, always-on-top window. Only basic vector primitives (ellipses, polygons, lines) are used — no blur or shadow effects — and the window's actual bounds and hit-test region are recomputed every frame from the bounding box of whatever gets drawn.
- **Animation**: fully procedural — IK, FABRIK, and Verlet chains, no baked animation frames. A character's "state" is mostly a handful of parameters that spring/ease toward target values, so transitions between states are smooth by construction.
- **Desktop awareness**: a `Desktop` layer turns the edges of every other on-screen window into `Surface`s that characters can walk on, cling to, or bridge between, while also tracking window move/open/close events that feed directly into character reactions.
- **Dependencies**: plain C++20, nothing beyond Win32/D3D/D2D.

### 📦 Download & run

**Prebuilt download**

Grab the zip from the Releases page, unzip it, and double-click `DesktopPetsGen3.exe` — no build tools needed.

**Build from source**

Requires Windows + MinGW-w64 (C++20-capable gcc) + CMake ≥ 3.24 + Ninja.

```bash
git clone <repo-url>
cd gen3
cmake -G Ninja -B build
cmake --build build
./build/DesktopPetsGen3.exe
```

Right-click the character to switch characters, resize, change the color palette, and more.

### 📁 Offline / portable

No network access, no registry writes, nothing dropped into your C: drive's AppData. All settings and save data live in a single `state.ini` next to `DesktopPetsGen3.exe`:

```
DesktopPetsGen3.exe
state.ini          ← auto-created on first run; positions/palette/save state live here
```

To move or back it up, just copy both files together; deleting `state.ini` resets everything.

### 🧠 Resource usage

Idles around 30–60MB of resident memory depending on which character is active and what's currently on screen (trails, seeds, particles bump it up briefly); CPU usage stays low, so it's fine to leave running in the background.

### 📄 License

[MIT](LICENSE)

Still iterating — behavior and tuning will keep changing.
