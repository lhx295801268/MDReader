# MDReader

跨平台 C++ Markdown 阅读/编辑工具。Qt 6 + cmark-gfm + QWebEngineView。

## 系统要求

### Linux
- **Ubuntu 22.04 LTS (Jammy)** 或更高,或 **Debian 12 (Bookworm)** 或更高
- 早于 Ubuntu 22.04 / Debian 12 的系统主源没有 `libqt6*` 包,需自行安装 Qt 6.5+(如 [aqtinstall](https://github.com/miuruaqt/aqtinstall))后用 `dpkg --force-depends` 强装本 .deb
- x86_64 架构

### Windows
- **Windows 10 64-bit (1809+)** / **Windows 11**,或 **Windows Server 2019+**
- NSIS 安装器由 Visual Studio 2022 + Qt 6.5 MSVC 2019 64-bit 构建

### macOS
- **macOS 11.0 (Big Sur) 或更高**;主要在 **macOS Sequoia 15.8.1** 上验证
- Apple Silicon(arm64)或 Intel(x86_64)64 位
- 发布的 `.dmg` 未做公证(notarization),首次打开需在「访达」里**右键 → 打开**,或在终端执行:
  ```bash
  xattr -dr com.apple.quarantine /Applications/MDReader.app
  ```
  这是所有未签名应用的通用提示,不是应用本身的缺陷

### 通用
- Qt 6.5 或更高(QWebEngineView 依赖 Chromium 内核)
- 大约 200 MB 磁盘空间(Qt WebEngine 进程资源)

## 截图

[docs/screenshots/main.png](docs/screenshots/main.png)

## 功能

- 横向分栏 + 可拖动分隔栏(大纲 / 编辑器 / 预览 / 信息)
- 实时预览(250 ms 去抖),支持手动模式
- 6 套主题(GitHub / GitHub Dark / Dracula / Solarized / One Dark)
- 数学公式(MathJax)、代码高亮(highlight.js)
- Mermaid 图表(流程图 / 思维导图 / 架构图)与图片支持**点击放大、滚轮缩放、拖动平移**,带可见工具栏(放大 / 缩小 / 适应窗口 / 1:1 / 关闭)
- 粘贴图片自动写入 `<doc>.assets/`
- 多 Tab + 会话恢复
- 文件被外部改动自动检测,冲突时弹三按钮 modal
- 导出为 HTML / PDF
- 菜单栏快捷键(`⌘N/O/S/W/Q`、`⌘F` 查找、`⌘R` 刷新、`⌃⌘F` 全屏);macOS 上自动使用系统菜单栏
- Linux `.deb`、Windows `.exe` 与 macOS `.dmg` 安装包

## 安装

### Linux(Ubuntu 22.04+ / Debian 12+)

```bash
sudo apt install ./MDReader-0.2.0-Linux.deb
mdreader your-notes.md
```

### Windows

下载 `MDReader-0.2.0-win64.exe`,双击安装。

### macOS(Sequoia 15.x)

下载 `MDReader-0.2.0-macOS.dmg`,打开后把 **MDReader.app** 拖进 **Applications** 即可。

之后可以直接双击 `.md` 文件打开(Finder 的「打开方式」里已经注册了 MDReader);应用已在运行时再双击其它文件,会通过 `QFileOpenEvent` 交给已有实例打开。

## 从源码构建

```bash
git clone --recursive https://github.com/your-org/MDReader.git
cd MDReader
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # 可选
./build/bin/mdreader your.md
```

### macOS

需要 Qt 6.5+(**含 WebEngine 模块**)。用 [aqtinstall](https://github.com/miuruaqt/aqtinstall) 安装最省事:

```bash
python3 -m pip install aqtinstall
python3 -m aqt install-qt mac desktop 6.5.3 clang_64 -m qtwebengine -O ~/Qt
```

然后:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$HOME/Qt/6.5.3/macos"
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/bin/mdreader your.md
```

打 `.dmg` 包(`macdeployqt` 会在 install 阶段自动运行,把 Qt 框架拷进 `.app`):

```bash
cd build && cpack -G DragNDrop
# → build/MDReader-0.2.0-macOS.dmg
```

产物位于 `build/MDReader.app`。图标可以用 CMake 目标从 PNG 重新生成(需要 macOS 自带的 `sips` / `iconutil`):

```bash
cmake --build build --target mdreader_regen_icon
```

## 截图存放

`docs/screenshots/` 下保存 6 张:全功能视图、浅色主题、深色主题、含数学公式、含代码块、文件冲突 modal。

## 第三方依赖与开源声明

MDReader 使用(并感谢)以下开源项目:

| 依赖 | 版本 | 许可证 | 用途 |
|---|---|---|---|
| [Qt](https://www.qt.io/) | 6.5+ | [LGPL v3](https://www.gnu.org/licenses/lgpl-3.0.html) / GPL v2+ / 商业 | 窗口、事件循环、网络、WebEngine 预览 |
| [cmark-gfm](https://github.com/github/cmark-gfm) | 0.29.0.gfm.13 | [BSD 2-Clause](https://github.com/github/cmark-gfm/blob/master/COPYING) | GitHub Flavored Markdown 解析 |
| [highlight.js](https://highlightjs.org/) | 11.9.0 | [BSD 3-Clause](https://github.com/highlightjs/highlight.js/blob/main/LICENSE) | 代码块语法高亮( vendored 在 `src/resources/vendor/highlight/`) |
| [MathJax](https://www.mathjax.org/) | 3 | [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) | 数学公式渲染( vendored 在 `src/resources/vendor/mathjax/`) |
| [Mermaid](https://mermaid.js.org/) | 11.17.2 | [MIT](https://github.com/mermaid-js/mermaid/blob/develop/LICENSE) | 流程图 / 思维导图 / 架构图渲染( vendored 在 `src/resources/vendor/mermaid/`) |

`.deb` 与 `.exe` 安装器均通过**动态链接** Qt,不修改 Qt 源码,因此 LGPL v3 兼容性条款得到满足;highlight.js、MathJax 与 Mermaid 以原始源码形式 vendored,许可文本保留在各文件中。

## 许可证

本项目以 [MIT 协议](LICENSE)开源。`Qt`(`LGPL v3`)、`cmark-gfm`(`BSD 2-Clause`)、`highlight.js`(`BSD 3-Clause`)、`MathJax`(`Apache 2.0`)、`Mermaid`(`MIT`)各自的协议文本分别随其原始代码分发,详见上方"第三方依赖与开源声明"。