# Desktop Pets (gen3)

> Three small creatures that live on the edges of your windows, each with its own temperament.

![platform](https://img.shields.io/badge/platform-Windows-0a84ff)
![language](https://img.shields.io/badge/language-C%2B%2B20-00599C)
![status](https://img.shields.io/badge/status-prototype-yellow)

[English](#english) | [中文](#中文)

---

## English

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

Not decided yet — don't use this commercially.

Still an actively evolving prototype; behavior and tuning will keep changing.

---

## 中文

### 🐛🪼🌿 三个角色

**🐛 机械蜘蛛** 会顺着窗口边缘巡视、检查，需要的时候真的会掏出工具干活——探测、夹东西、拧紧、点焊、刷一下收尾，每个动作都对应它正在处理的状况。窗口刚被挪动过的地方会被它优先过去看看；慢慢拖着它走和手一甩它的反应也不一样；脚下踩着东西的时候会有点贴地的影子。右键菜单里能调它干活的勤快程度。

**🪼 游隙** 没有固定的身体形状，膜和触丝会随着它正在做的事持续变形——加速、转弯、被抓住、被放开，看起来都不一样，过渡都是软的。它会自己织巢、整理自己；身体中间有个口子，鼠标靠近了它会转过去对准你，穿过去会有明显反应；偶尔还会自己回头追自己刚划过的痕迹，或者借着你拖窗口带起的风绕一圈再飞回来。

**🌿 栖枝** 的枝条会自己找地方攀附，离得近的两扇窗口之间甚至能搭出一座桥，窗口拉远了桥会慢慢松开。花按自己的节奏开谢、结出带翅膀的种子，风吹过来的种子它也会试着接住。拖着它的根搬家时枝叶会先收拢再跟着晃；缠住的卷须也可以直接拽那段线让它自己解开。

### 🏗️ 技术架构

- **渲染**：DirectComposition + Direct3D 11 + Direct2D，透明、点击穿透、始终置顶的窗口；只用椭圆/多边形/线条这类基础矢量图元，没有模糊或阴影特效，窗口的实际区域和可点击范围是每帧根据绘制内容的包围盒现算出来的。
- **动画**：纯程序化——IK、FABRIK、Verlet 链，没有预烘焙的动画帧。角色的"状态"几乎都是一组会用弹簧/指数缓动追赶目标值的参数，所以状态之间的过渡天然是平滑的。
- **桌面感知**：`Desktop` 这一层把屏幕上其它窗口的边缘抽象成一条条可以附着、行走、搭桥的 `Surface`，同时追踪窗口的移动、打开、关闭事件，这些事件会直接触发角色的反应。
- **依赖**：纯 C++20，没有除 Win32/D3D/D2D 之外的第三方库。

### 📦 下载与运行

**直接下载**

如果仓库有发布 Release，就去 Releases 页面下载打包好的压缩包，解压后双击 `DesktopPetsGen3.exe` 就能跑，不需要装任何编译工具。（目前还没有正式发布过 Release，这里先占个位置，发布之后会把下载链接补在这。）

**从源码构建**

需要 Windows + MinGW-w64（gcc 支持 C++20）+ CMake ≥ 3.24 + Ninja。

```bash
git clone <仓库地址>
cd gen3
cmake -G Ninja -B build
cmake --build build
./build/DesktopPetsGen3.exe
```

右键点击角色可以切换角色、调整大小和配色，机械蜘蛛还能单独调它干活的勤快程度。

### 📄 许可

还没定，先别拿去商用。

仍是持续迭代中的原型，行为和数值都可能继续调整。
