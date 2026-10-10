# Desktop Pets（gen3）

> IK程序化生成验证桌宠。

![platform](https://img.shields.io/badge/platform-Windows-0a84ff)
![language](https://img.shields.io/badge/language-C%2B%2B20-00599C)
![license](https://img.shields.io/badge/license-MIT-green)

<p align="center">
  <img src="monika.png" width="180" alt="项目吉祥物斜眼看你">
  <br>
  <sub>这是项目吉祥物，正斜眼看着你有没有认真读文档。</sub>
</p>

EN：[English](README.en.md)

---

### 🐛🪼🌿 三个角色

**🐛 机械蜘蛛** 会顺着窗口边缘巡视、检查，右键菜单里能调它“干活”的勤快程度。

**🪼 游隙** 没有固定的身体形状，膜和触丝会随着它正在做的事持续变形，我梦里出现的神秘生物。

**🌿 栖枝** 是一颗花草

### 🏗️ 技术架构

- **渲染**：DirectComposition + Direct3D 11 + Direct2D，透明、点击穿透、始终置顶的窗口；只用椭圆/多边形/线条这类基础矢量图元，没有模糊或阴影特效，窗口的实际区域和可点击范围是每帧根据绘制内容的包围盒现算出来的。
- **动画**：纯程序化——IK、FABRIK、Verlet 链，没有预烘焙的动画帧。角色的"状态"几乎都是一组会用弹簧/指数缓动追赶目标值的参数，所以状态之间的过渡天然是平滑的。
- **桌面感知**：`Desktop` 这一层把屏幕上其它窗口的边缘抽象成一条条可以附着、行走、搭桥的 `Surface`，同时追踪窗口的移动、打开、关闭事件，这些事件会直接触发角色的反应。
- **依赖**：纯 C++20，没有除 Win32/D3D/D2D 之外的第三方库。

### 📦 下载与运行

**直接下载**

Releases 页面下载打包好的压缩包，解压后双击 `DesktopPetsGen3.exe` 就能跑，不需要装任何编译工具。

**从源码构建**

需要 Windows + MinGW-w64（gcc 支持 C++20）+ CMake ≥ 3.24 + Ninja。

```bash
git clone <仓库地址>
cd gen3
cmake -G Ninja -B build
cmake --build build
./build/DesktopPetsGen3.exe
```

右键点击角色可以切换角色、调整大小和配色等。

### 📁 离线 / 便携

不联网、不写注册表、不往 C 盘 AppData 塞东西。所有设置和存档都保存在 `DesktopPetsGen3.exe` 同级目录下的一个 `state.ini` 里：

```
DesktopPetsGen3.exe
state.ini          ← 首次运行自动生成，角色位置/配色/存档都在这
```

想搬家或者备份，把这两个文件一起拷走就行；`state.ini` 删掉就等于重置。

### 🧠 资源占用

常驻内存大概 30～60MB，具体看当前角色和屏幕上有什么（残影、种子、粒子这类临时对象会短暂拉高一点），CPU 占用很低，挂在后台不会有什么负担。

### 📄 许可

[MIT](LICENSE)

迭代中，行为和数值都可能继续调整。
