# MDReader

跨平台 C++ Markdown 阅读/编辑工具。Qt 6 + cmark-gfm + QWebEngineView。

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

## 许可证

MIT(待替换为实际许可证)。