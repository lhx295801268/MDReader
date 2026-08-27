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
- 粘贴图片自动写入 `<doc>.assets/`
- 多 Tab + 会话恢复
- 文件被外部改动自动检测,冲突时弹三按钮 modal
- 导出为 HTML / PDF
- Linux `.deb` 与 Windows `.exe` 安装器

## 安装

### Linux(Ubuntu 22.04+ / Debian 12+)

```bash
sudo apt install ./MDReader-0.1.0-Linux.deb
mdreader your-notes.md
```

### Windows

下载 `MDReader-0.1.0-win64.exe`,双击安装。

## 从源码构建

```bash
git clone --recursive https://github.com/your-org/MDReader.git
cd MDReader
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # 可选
./build/bin/mdreader your.md
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

`.deb` 与 `.exe` 安装器均通过**动态链接** Qt,不修改 Qt 源码,因此 LGPL v3 兼容性条款得到满足;highlight.js 与 MathJax 以原始源码形式 vendored,许可文本保留在各文件中。

## 许可证

本项目以 [MIT 协议](LICENSE)开源。`Qt`(`LGPL v3`)、`cmark-gfm`(`BSD 2-Clause`)、`highlight.js`(`BSD 3-Clause`)、`MathJax`(`Apache 2.0`)各自的协议文本分别随其原始代码分发,详见上方"第三方依赖与开源声明"。