# MDReader — 设计规范

> **日期**:2026-08-26
> **主题**:用 C++ 构建一个跨平台的 Markdown 阅读/编辑工具,名为 MDReader
> **技术栈**:Qt 6 Widgets + cmark-gfm + QWebEngineView(Chromium 内核)

## 1. 目标与非目标

### 目标
- 横向分栏布局:左侧源码、右侧渲染预览;用户可拖动分隔栏调整比例。
- 编辑时实时预览(去抖 ~250ms);文件被外部程序修改时自动同步;若应用内已有未保存编辑,则弹窗由用户裁决。
- 使用同一份 CMake 工程,既能打包成 Linux `.deb`,也能打包成 Windows `.exe` 安装程序。
- UI 风格参考 macOS 下的 MacDown.app:多 Tab、大纲(TOC)侧栏、信息侧栏、多套主题、代码块语法高亮、数学公式(LaTeX)、粘贴图片直接渲染。

### 非目标
- 云同步、账号、埋点。
- 所见即所得编辑(Markdown 源码即真理来源)。
- VCS 集成(只负责普通文件读写)。
- 插件系统、脚本 API。
- 移动平台。

## 2. 已决议项(brainstorming 阶段产出)

| # | 决策点 | 结果 |
|---|---|---|
| 1 | 功能范围 | **全套 MacDown 风格**:多 Tab、大纲 + 信息侧栏、主题、代码高亮、数学公式、粘贴图片(选项 D)。 |
| 2 | UI 框架 | **Qt 6 Widgets + QWebEngineView**(选项 A)。 |
| 3 | Markdown 解析库 | **cmark-gfm**(GitHub Flavored Markdown;选项 A)。 |
| 4 | 外部修改文件时的行为 | **智能策略**:编辑实时去抖渲染;未脏则自动重载;脏则弹三按钮 modal(丢弃 / 保留 / 备份后加载)。 |
| 5 | 主界面布局 | **C 方案(全显式面板)**:大纲(左,可隐藏)+ 编辑器(可拖)+ 预览(可拖)+ 信息(右,可隐藏)。 |
| 6 | 去抖时长 | 编辑器输入 → 250ms 后渲染;文件监视事件 → 500ms 后再触发外部变动逻辑。 |
| 7 | 图片资产落盘位置 | 与文档同目录的 `.assets/<sha256>.<ext>`,随文档一起移动。 |
| 8 | 导出 | 同时支持 HTML 与 PDF(PDF 用 `QWebEnginePage::printToPdf()`)。 |

## 3. 技术栈

- C++17,CMake ≥ 3.21。
- Qt 6.5+(组件:`Widgets`、`WebEngineWidgets`)。
- cmark-gfm — 用 CMake `FetchContent` 拉取,源代码放在 `vendor/cmark-gfm/`。
- MathJax 3(tex-mml-chtml)— vendored 在 `src/resources/vendor/mathjax/`。
- highlight.js 11 — vendored 在 `src/resources/vendor/highlight/`。
- 主题(CSS)— vendored 在 `src/resources/themes/`,包含:`github.css`、`github-dark.css`、`dracula.css`、`solarized-light.css`、`solarized-dark.css`、`one-dark.css`。
- 构建工具:CMake;打包:CPack(Linux 出 DEB,Windows 出 NSIS)。Windows 必须在 Windows 上原生编译(Qt WebEngine 交叉编译代价过高)。

## 4. 架构

轻度 MVC + 小型 services 层。

```
┌──────────────────────────────────────────────────────────────────┐
│ MainWindow  (菜单、工具栏、QTabWidget 宿主、QSettings I/O)         │
│   └── DocumentTab × N                                           │
│         ├── QSplitter A  [ 大纲视图 │ 主体分隔栏 ]               │
│         │                  ├── QSplitter B                       │
│         │                  │     ├── 编辑器视图                  │
│         │                  │     ├── QSplitter C                 │
│         │                  │     │     ├── 预览视图              │
│         │                  │     │     └── 信息视图              │
│   └── DocumentManager(打开/保存、当前选择、会话持久化)            │
│                                                                  │
│ Services(无 UI)                                                 │
│   ├── MarkdownRenderer   cmark-gfm → HTML + 注入 CSS/JS         │
│   ├── RenderCoordinator  250ms 去抖 & 手动模式门控               │
│   ├── FileWatcher        QFileSystemWatcher + 500ms 去抖         │
│   ├── OutlineExtractor   markdown 文本 → 标题树模型              │
│   ├── WordCounter        字数 / 字符数 / 段落数 / 标题数          │
│   └── ImageHandler       拖入/粘贴图像 → 同目录资产             │
│                                                                  │
│ Resources                                                         │
│   └── resources.qrc       themes/、vendor/{highlight,mathjax}/,  │
│                            icons/                                │
└──────────────────────────────────────────────────────────────────┘
```

### 4.1 模块职责

- **`Document`** — 一份文档的状态:文件路径、当前文本、上次已存文本、`dirty` 标记、最近 mtime。身份用绝对路径区分。仅数据,不依赖 Qt 控件。
- **`DocumentManager`** — 持有 `Document*` 列表与当前索引,负责 open/save/close、关闭时处理 dirty 状态、会话恢复(QSettings)。
- **`EditorView`** — `QPlainTextEdit` 子类:等宽字体、自绘行号侧栏(在 `paintEvent` 内画)、处理 image MIME。
- **`PreviewView`** — `QWebEngineView` 包装类,`setMarkdown(text)` 会构造 HTML 文档(渲染体 + 主题 CSS + 第三方 JS 标签)。对外暴露 `exportHtml(path)`(写当前渲染好的 HTML)和 `exportPdf(path)`(调用 `QWebEnginePage::printToPdf()`)。
- **`OutlineView`** — `QTreeView`,从 `OutlineExtractor` 拿到的标题模型读出;emit `headingActivated(lineNumber, id)`。
- **`InfoView`** — `QFormLayout` 风格的小面板,显示 `WordCounter` 输出的统计。
- **`DocumentTab`** — 用三层嵌套 QSplitter 组装四个子视图。
- **`MainWindow`** — 工具栏动作、菜单、主题菜单、`QTabWidget`、splitter/state 的保存与恢复(均走 QSettings)。
- **`MarkdownRenderer`** — 拥有 `cmark_parser`;文本转 HTML;追加 `<style>`(主题)和 `<script>`(highlight.js + MathJax);保留原文 `$$…$$` / `$…$` 给 MathJax typeset。
- **`RenderCoordinator`** — 持有一个 `QTimer`;监听 `EditorView::textChanged` 并以 250ms 重启单次定时器;手动模式下定时器被禁用,只在用户点 "Refresh" 时触发。
- **`FileWatcher`** — 单个 `QFileSystemWatcher`;每条被监视路径一个 500ms `QTimer`,合并多次事件为一次 `externalModified(docId, newBytes)`。
- **`OutlineExtractor`** — 用正则 `^(#{1,6})\s+(.+)$` 扫一遍文本,返回 `QStandardItemModel`。
- **`WordCounter`** — 按需统计;纯函数。
- **`ImageHandler`** — 给定 `QByteArray`,计算 SHA-256,写到 `<dir>/<basename>.assets/<hash>.<ext>`,返回相对 markdown 引用路径。

### 4.2 编辑 ↔ 预览的数据流

```
EditorView.textChanged
        │
        ▼
RenderCoordinator.requestRender(text)
        │  (250ms QTimer;单次)
        ▼
MarkdownRenderer.render(text, themeId) ──► HTML 字符串 + 主题 CSS 注入
        │
        ▼
PreviewView.setHtml(html, baseUrl=QUrl("qrc:///"))
        │
        ▼
QWebEngineView 页面; DOMContentLoaded 后 highlight.js 执行;
其完成后再调用 MathJax.typesetPromise()。
```

### 4.3 外部修改文件的处理

```
FileWatcher.externalModified(docId, bytes)
        │
        ▼
DocumentManager.onExternalChange(docId, bytes)
        ├── 若 document.dirty == false:
        │     EditorView.setPlainText(bytes); RenderCoordinator.requestRender();
        │     状态栏提示 "已从磁盘重新加载"
        └── 若 document.dirty == true:
              QMessageBox(三按钮,默认 = "丢弃并重载"):
                [ 丢弃我的编辑并重新加载 ]
                  → 丢弃内存内容;把磁盘字节重新载入编辑器;重新渲染。
                [ 保留我的编辑(忽略磁盘改动) ]
                  → 保留内存内容;对该路径临时取消文件监视,
                    直到用户主动保存为止,不再响应后续磁盘事件。
                [ 备份我的编辑然后重新加载 ]
                  → 把当前内存内容先写到
                    `<doc>.conflict-<UTC-timestamp>.md` 同目录备份,
                    然后把磁盘字节载入编辑器并重新渲染。
                    外部版本成为权威,本机编辑保留在备份文件中。
```

### 4.4 图像粘贴流程

```
EditorView.insertFromMimeData() 识别到 image MIME
        │
        ▼
ImageHandler.handle(rawBytes, suggestedName)
        │
        ▼ (写入 <docDir>/<basename>.assets/<sha256>.<ext>)
返回相对 markdown 引用路径
        │
        ▼
EditorView.insertText("![](" + ref + ")")      ← 并立即触发一次渲染
```

## 5. 布局与分隔栏持久化

### 5.1 分隔栏拓扑(每个 Tab 内)

```
QSplitter A   (水平;持久化键 "layout/splitterA")
├── 大纲视图                       (可隐藏;最小 120)
└── QSplitter B  (水平;持久化键 "layout/splitterB")
    ├── 编辑器视图                 (最小 200)
    └── QSplitter C  (水平;持久化键 "layout/splitterC")
        ├── 预览视图               (最小 200)
        └── 信息视图               (可隐藏;最小 150)
```

### 5.2 分隔栏行为
- 鼠标悬停分隔栏手柄时,光标变为 `Qt::SplitHCursor`。
- 拖动时被钳到各子项的最小宽度;不允许用拖动把任一面拖到 0。
- 工具栏上的 "☐ 大纲" / "☐ 信息" 复选框用于整体显隐(不走分隔栏把手),显隐状态会被记住。

### 5.3 持久化
在 `MainWindow::closeEvent`(以及 Tab 关闭、主题切换时)保存以下键到 QSettings:
- `MDReader/layout/splitterA/B/C` — `QByteArray`,由 `saveState()` 产生。
- `MDReader/sidebar/outlineVisible`、`MDReader/sidebar/infoVisible` — bool。
- `MDReader/session/lastFiles` — `QStringList`,打开的 Tab 绝对路径列表。
- `MDReader/session/currentIndex` — int。
- `MDReader/ui/theme` — `QString`(主题名之一)。首次运行默认 `github`。
- `MDReader/ui/renderMode` — `"live"`(默认)或 `"manual"`。
- `MDReader/ui/showLineNumbers` — bool。首次运行默认 `true`。

启动时 `MainWindow` 按上述顺序读取并应用;遇到路径已不存在的,跳过并记录一条 Warning。

## 6. 构建、安装、打包

### 6.1 Linux 构建
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCPACK_PACKAGE_VERSION=0.1.0
cmake --build . -j
cpack -G DEB
```
产物:`MDReader-0.1.0-Linux.deb`。运行时依赖系统 Qt 6.5+ 包:`libqt6core6`、`libqt6gui6`、`libqt6widgets6`、`libqt6webengine6`、`libqt6webenginewidgets6`。

### 6.2 Windows 构建(必须在 Windows 上原生编译)
```bat
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j
windeployqt mdreader.exe
cpack -G NSIS -C Release
```
产物:`MDReader-0.1.0-win64.exe`。NSIS 产出自包含安装器;`windeployqt` 复制所有 Qt DLL 与插件。

### 6.3 Linux `.deb` 安装目录

| 文件 | 路径 |
|---|---|
| 可执行文件 | `/usr/bin/mdreader` |
| 桌面入口 | `/usr/share/applications/mdreader.desktop` |
| 图标(128×128) | `/usr/share/icons/hicolor/128x128/apps/mdreader.png` |
| MIME | `/usr/share/mime/packages/mdreader-mime.xml` |

`postinst` 触发 `update-mime-database` 和 `update-desktop-database`。目标发行版:Ubuntu 22.04 LTS、Debian 12、Linux Mint 21+。

### 6.4 Windows 安装目录
NSIS 安装器把 `mdreader.exe` 与所有 Qt DLL 装到 `C:\Program Files\MDReader\`,并注册 `.md` 文件的 shell 关联。

### 6.5 内嵌资源
MathJax 与 highlight.js 全部内嵌在 `src/resources/vendor/` 下,通过 `qrc:/vendor/...` 加载。运行时不需要任何网络访问。

## 7. 错误处理

| 场景 | 处理 |
|---|---|
| 打开文件失败(不存在 / 无权限) | 状态栏红色错误;不创建 Tab。 |
| 文件被外部删除 | 状态栏红色提示;Tab 保留内存内容直到用户决定。 |
| 打开文件 >10MB | 弹确认对话框 `继续?`(默认 = 继续)。渲染节流改为 750ms。 |
| 粘贴图片写盘失败(磁盘满 / 目录只读) | 状态栏红色;markdown 引用回滚。 |
| `cmark-gfm` 返回 null / 报错 | `MarkdownRenderer` 内捕获;保留上一次成功预览;状态栏报"渲染失败"。 |
| 启动期 MathJax / highlight.js 加载失败 | 顶部黄色条幅:`代码高亮或数学公式不可用`;预览主体仍正常。 |
| 分隔栏保存的尺寸之和为 0 | 使用默认值(大纲 200 / 编辑器 50% / 预览 50% / 信息 220)。 |
| 多个 Tab 监听同一文件路径 | 仅第一个 Tab 持有该路径的 watch;其它 Tab 通过应用内信号同步。 |
| 嵌入图片路径不存在 | 预览显示破图图标;非致命。 |

## 8. 测试

### 8.1 单元测试(Qt Test,放在 `tests/unit/`)
- `MarkdownRendererTest` — GFM 表格、GFM 任务列表、带语言的围栏代码块、保留 `$$…$$` 数学块原文。
- `OutlineExtractorTest` — 从 fixture 提取 H1–H6 且顺序正确。
- `WordCounterTest` — 中英文字混合统计。
- `FileWatcherDebounceTest` — 300ms 内 5 次事件只产生 1 次 `externalModified`。
- `ImageHandlerTest` — 相同字节同 hash;不同字节不同文件。

### 8.2 集成测试(Qt Test,放在 `tests/integration/`)
- `EditToPreviewTest` — `EditorView.setPlainText(text)` ⇒ 等 `RenderCoordinator` tick + `QWebEnginePage::loadFinished` ⇒ 把 `page().mainFrame().toHtml()` 与期望子串比较。
- `ThemeSwitchTest` — `ThemeMenu::setTheme("dracula")` 后,预览 HTML 的 `<style>` 包含 Dracula 类名。
- `ExternalReloadTest` — 设为 dirty,从外部写入字节,期望走 modal 分支(用注入 mock 对话框验证)。

### 8.3 手工冒烟(Linux 与 Windows 各执行一遍)
- [ ] 通过 `.desktop` / 开始菜单启动。
- [ ] 通过 GUI 打开 `.md` 文件;通过命令行(`mdreader foo.md`)打开。
- [ ] 编辑、保存、重开 — 往返一致。
- [ ] 在 MDReader 编辑同一文件,同时在 Vim 编辑并保存 — 出现带三按钮的 modal。
- [ ] 拖动分隔栏 — 重启后比例保持。
- [ ] 切换显隐大纲 / 信息 — 状态保持。
- [ ] 切换主题 — 预览切换,编辑器不变。
- [ ] 粘贴图片 — 预览中可见。
- [ ] 渲染含 `$$E=mc^2$$` 与围栏 ` ```python ` 的 fixture。
- [ ] 导出 PDF 与 HTML。
- [ ] 截图归档到 `docs/screenshots/`。

## 9. 验收标准

1. `cmake --build` 在 Linux 与 Windows 都产出可运行的二进制。
2. `cpack` 在两平台各自产出可用的 `.deb` 与 `.exe` 安装包。
3. 打开 1MB Markdown:首次渲染 < 500ms;按键到预览更新 p95 < 300ms。
4. 外部编辑 + 未脏 → 预览自动更新。
5. 外部编辑 + 已脏 → 三按钮 modal 出现,语义与默认按钮正确。
6. 分隔栏尺寸、主题、渲染模式、上次打开文件、侧栏显隐 — 重启后全部恢复。
7. `$$…$$` 数学公式与围栏代码块,在 `highlight.js` / `MathJax` 加载完成后正确渲染。
8. 拖入 / 粘贴图片写入 `<dir>/<basename>.assets/<sha256>.<ext>`,预览渲染正常。
9. `cpack -G DEB` 与 `cpack -G NSIS` 不需人工干预即可产出可安装的包。
10. 单元 + 集成测试在 CI 上全部通过。

## 10. 实施阶段

| 阶段 | 内容 | 工作日 |
|---|---|---|
| 0 | 仓库脚手架;`CMakeLists.txt`;Qt 项目骨架;空白主窗口能跑。 | 1 |
| 1 | `Document` + `EditorView` + `PreviewView` + `MarkdownRenderer` + `RenderCoordinator`(250ms 去抖)。 | 3 |
| 2 | `DocumentTab` 中三层嵌套 QSplitter;`OutlineView` + `InfoView`;`QSettings` 持久化;多 Tab;会话恢复。 | 2 |
| 3 | 多主题(5+)菜单;实时 / 手动渲染模式切换;大纲 ↔ 编辑器联动(点击 + 光标);highlight.js;MathJax;图片粘贴。 | 3 |
| 4 | `FileWatcher` + 500ms 去抖 + 冲突 modal + HTML 导出 + PDF 导出。 | 1 |
| 5 | CPack DEB + CPack NSIS + 图标 / `.desktop` / MIME 注册。 | 2 |
| 6 | 单元 + 集成测试 + Linux/Windows 冒烟 + README。 | 1 |
| **合计** | | **≈ 13 个工作日(≈ 2.5 周)** |

## 11. 仓库目录结构

```
MDReader/
├── CMakeLists.txt
├── cmake/                      # CPack 辅助脚本
├── docs/
│   ├── superpowers/specs/      # 本文件
│   └── screenshots/
├── src/
│   ├── CMakeLists.txt
│   ├── app/{main.cpp, MainWindow.{h,cpp}}
│   ├── documents/{Document.{h,cpp}, DocumentManager.{h,cpp}}
│   ├── ui/{EditorView, PreviewView, OutlineView, InfoView, DocumentTab, ThemeMenu}.{h,cpp}
│   ├── services/{MarkdownRenderer, RenderCoordinator, FileWatcher,
│   │             OutlineExtractor, WordCounter, ImageHandler}.{h,cpp}
│   └── resources/
│       ├── resources.qrc
│       ├── themes/{github, github-dark, dracula, solarized-light, solarized-dark, one-dark}.css
│       ├── vendor/{highlight, mathjax}/…
│       └── icons/
├── tests/{unit, integration}/
├── vendor/cmark-gfm/           # CMake FetchContent 自动拉取
└── .gitignore
```

## 12. 待用户提出的问题

_(spec 撰写时无 — 全部决策已落定)_
