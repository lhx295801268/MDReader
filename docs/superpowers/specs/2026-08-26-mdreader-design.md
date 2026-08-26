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

### 4.0 线程模型

为避免大文件(>1MB)渲染或文件 IO 阻塞 GUI 主线程,明确采用以下线程划分:

| 组件 / 动作 | 所在线程 |
|---|---|
| `MainWindow`、`EditorView`、`PreviewView`、所有 `QWidget`、`QSplitter`、`QTabWidget`、菜单 / 工具栏 | **GUI 主线程**(Qt 的规定) |
| `Document` 的字段读 / 写 | 通过 `std::shared_mutex`(`QReadWriteLock`)保护,任何线程都可调用 |
| `QFileSystemWatcher` 信号投递 | 主线程(Qt 内部规则) |
| 大文件 / 慢 IO 的字节读取 | **Worker 线程池**(`QThreadPool::globalInstance()`);读完后通过 `Qt::QueuedConnection` 信号回到主线程 |
| `MarkdownRenderer::render()` 渲染主体 | **Worker 线程池**;HTML 字符串通过 `QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection)` 回到主线程交付给 `PreviewView` |
| `OutlineExtractor::extract()`、`WordCounter::count()`、`ImageHandler::handle()` | 纯函数,可在任何线程调用;由调用方决定线程归属 |
| `RenderCoordinator` 的 `QTimer` 与去抖决策 | 主线程(只持有 timer 状态);真正渲染通过 `QtConcurrent::run` 推到 worker |

**结果安全保证**:
- `Document::text()` 内部获取**读锁**(`std::shared_lock`)返回文本副本(避免悬空),调用方拿到的 `QString` 独立于内部缓冲。
- `Document::setText()` 内部获取**写锁**(`std::unique_lock`),先复制再覆盖。
- 渲染 worker 拿走的是 `QString` 副本,与后续编辑无关。
- 主线程在 `setText` / 接收到渲染结果更新 `EditorView` / `PreviewView` 时不持任何锁;Qt 控件天然由调用者所在线程(即主线程)独占。
- 三按钮 modal 的判定发生在主线程(`onExternalChange` 回调就是主线程信号),不涉及共享状态。

**错误情况**:
- Worker 上抛异常(罕见)→ 通过 `Qt::QueuedConnection` 投递到主线程的 `onRenderFailed(text)` 信号,`PreviewView` 保留上一次成功渲染,状态栏报"渲染失败"。
- 读盘 IO 失败 / 超时(>5s)→ 同上路径,主线程弹一次状态栏红字。



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

- **`Document`** — 一份文档的状态:文件路径、当前文本、上次已存文本、`dirty` 标记、最近 mtime。身份用绝对路径区分。**内部用 `std::shared_mutex` 保护文本缓冲**:`text()` 返回 `QString` 副本(读锁)、`setText()` 整体替换(写锁)、`saveToDisk()` 由调用方自己负责。仅数据,不依赖 Qt 控件。
- **`DocumentManager`** — 持有 `Document*` 列表与当前索引,负责 open/save/close、关闭时处理 dirty 状态、会话恢复(QSettings)。**文件 IO 路径(打开大文件读字节、保存到磁盘)通过 `QtConcurrent::run` 推到 worker 线程,完成后用 `Qt::QueuedConnection` 信号把字节回投主线程**。
- **`EditorView`** — `QPlainTextEdit` 子类:等宽字体、自绘行号侧栏(在 `paintEvent` 内画)、处理 image MIME。仅主线程使用。
- **`PreviewView`** — `QWebEngineView` 包装类,`setMarkdown(text)` 会构造 HTML 文档(渲染体 + 主题 CSS + 第三方 JS 标签);`setHtml(htmlString)` 由 `RenderCoordinator` 在主线程调用。对外暴露 `exportHtml(path)`(写当前渲染好的 HTML)和 `exportPdf(path)`(调用 `QWebEnginePage::printToPdf()`)。仅主线程使用。
- **`OutlineView`** — `QTreeView`,从 `OutlineExtractor` 拿到的标题模型读出;emit `headingActivated(lineNumber, id)`。主线程。
- **`InfoView`** — `QFormLayout` 风格的小面板,显示 `WordCounter` 输出的统计。主线程。
- **`DocumentTab`** — 用三层嵌套 QSplitter 组装四个子视图。主线程。
- **`MainWindow`** — 工具栏动作、菜单、主题菜单、`QTabWidget`、splitter/state 的保存与恢复(均走 QSettings)。主线程。
- **`MarkdownRenderer`** — 拥有 `cmark_parser`;文本转 HTML;追加 `<style>`(主题)和 `<script>`(highlight.js + MathJax);保留原文 `$$…$$` / `$…$` 给 MathJax typeset。**线程安全,设计为"任何线程可调用"**(内部状态由互斥锁保护或完全函数式)。
- **`RenderCoordinator`** — 主线程持 `QTimer`,250ms 去抖;**真正调用 `MarkdownRenderer::render()` 时通过 `QtConcurrent::run` 推到全局线程池**,完成后用 `Qt::QueuedConnection` 把 HTML 字符串投给目标 `PreviewView::setHtml()`。同一文档若新一帧渲染还在跑就丢弃旧帧。
- **`FileWatcher`** — 主线程维护 `QFileSystemWatcher`,信号在主线程投递;**实际读取磁盘字节通过 `QtConcurrent::run` 推到 worker**(>1MB 时必需);读完后回投主线程 emit `externalModified(docId, newBytes)`;500ms `QTimer` 用于合并多次底层事件。读盘超时 5s 视为失败。
- **`OutlineExtractor`** — 纯函数,可在任意线程调用;主线程 / worker 均可。
- **`WordCounter`** — 纯函数,可在任意线程调用。
- **`ImageHandler`** — SHA-256 计算 + 文件写入;**写盘动作在主线程同步执行**(避免与其他文件 IO 竞争,且写盘量很小一般 <10MB);调用频率低,主线程开销可接受。

### 4.2 编辑 ↔ 预览的数据流

```
[主线程] EditorView.textChanged
            │
            ▼
        RenderCoordinator::requestRender(docId)           ← 重启 250ms QTimer
            │
   [250ms 后,主线程触发 timer]
            │
            ▼
        RenderCoordinator 用 QtConcurrent::run 推到
        全局 QThreadPool 的 Worker 上,启动渲染任务
            │
[Worker 线程] MarkdownRenderer::render(text, themeId)
            │   (cmark-gfm 解析 + CSS/JS 拼装)
            ▼
        HTML 字符串 ── Qt::QueuedConnection ──┐
            │                                  ▼
            │                          [主线程] PreviewView::setHtml(html)
            │                                  │
            │                                  ▼
            │                          QWebEngineView 页面;
            │                          DOMContentLoaded 后 highlight.js 执行;
            │                          其完成后再调用 MathJax.typesetPromise()。
            │
            ▼
        期间若新的 requestRender 到达,旧 worker 任务结束后的结果
        投回主线程时会被 RenderCoordinator 的"帧序号"检查丢弃,
        避免旧 HTML 覆盖新 HTML。
```

### 4.3 外部修改文件的处理

```
[主线程] QFileSystemWatcher 信号
            │
            ▼
        FileWatcher::onFsEvent(path)   ── 重启 500ms 路径级 QTimer
            │
[500ms 后,主线程] 触发实际读盘
            │
            ▼
        FileWatcher 用 QtConcurrent::run 推到 Worker,
        读 path 的字节(QFile + 5s 超时)
            │
[Worker 线程] 读盘完成(>1MB 时不会阻塞 UI)
            │
   Qt::QueuedConnection 回投主线程
            │
            ▼
[主线程] FileWatcher::onReadDone(path, bytes)
            │
            ▼
        DocumentManager::onExternalChange(docId, bytes)
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
- `MarkdownRendererTest` — GFM 表格、GFM 任务列表、带语言的围栏代码块、保留 `$$…$$` 数学块原文;同一 renderer 实例并发调用不破坏(线程安全)。
- `OutlineExtractorTest` — 从 fixture 提取 H1–H6 且顺序正确。
- `WordCounterTest` — 中英文字混合统计。
- `FileWatcherDebounceTest` — 300ms 内 5 次事件只产生 1 次 `externalModified`。
- `ImageHandlerTest` — 相同字节同 hash;不同字节不同文件。
- `DocumentLockTest` — 多线程并发 `setText` / `text()`,验证返回的 `QString` 永不破坏(`QString::isNull` 但内容完整);`std::shared_mutex` 配对正确。
- `RenderFrameDropTest` — 渲染一帧前先抛一个新 `requestRender`,旧帧结果应被丢弃(帧序号机制)。

### 8.2 集成测试(Qt Test,放在 `tests/integration/`)
- `EditToPreviewTest` — `EditorView.setPlainText(text)` ⇒ 等 `RenderCoordinator` tick + `QWebEnginePage::loadFinished` ⇒ 把 `page().mainFrame().toHtml()` 与期望子串比较。
- `ThemeSwitchTest` — `ThemeMenu::setTheme("dracula")` 后,预览 HTML 的 `<style>` 包含 Dracula 类名。
- `ExternalReloadTest` — 设为 dirty,从外部写入字节,期望走 modal 分支(用注入 mock 对话框验证)。
- `RenderCoordinatorWorkerTest` — 在主线程触发 render,期间 mock 一个 200ms 阻塞渲染,确认主线程 timer / UI 仍然响应(>1Hz),不被 worker 阻塞。

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
10. 单元 + 集成测试(含多线程并发测试)在 CI 上全部通过。
11. **线程验证**:连续狂输入 1MB+ Markdown,UI 不卡顿;主线程任意时刻响应 < 50ms;渲染 worker 偶发异常不会让 `PreviewView` 显示空白(回退到上一次成功的 HTML)。

## 10. 实施阶段

| 阶段 | 内容 | 工作日 |
|---|---|---|
| 0 | 仓库脚手架;`CMakeLists.txt`;Qt 项目骨架;空白主窗口能跑。 | 1 |
| 1 | `Document`(含 `std::shared_mutex`)+ `EditorView` + `PreviewView` + `MarkdownRenderer` + `RenderCoordinator`(250ms 去抖 + worker 渲染)。 | 3 |
| 1.5 | **线程基础设施**:`QtConcurrent::run` 集成、`Document` 锁、`RenderCoordinator` 的"帧序号丢弃"机制、worker 异常回主线程的统一通道。 | 1 |
| 2 | `DocumentTab` 中三层嵌套 QSplitter;`OutlineView` + `InfoView`;`QSettings` 持久化;多 Tab;会话恢复。 | 2 |
| 3 | 多主题(5+)菜单;实时 / 手动渲染模式切换;大纲 ↔ 编辑器联动(点击 + 光标);highlight.js;MathJax;图片粘贴。 | 3 |
| 4 | `FileWatcher`(主线程监视 + worker 读盘)+ 500ms 去抖 + 冲突 modal + HTML 导出 + PDF 导出。 | 1 |
| 5 | CPack DEB + CPack NSIS + 图标 / `.desktop` / MIME 注册。 | 2 |
| 6 | 单元 + 集成测试(含多线程并发测试)+ Linux/Windows 冒烟 + README。 | 1 |
| **合计** | | **≈ 14 个工作日(≈ 2.5–3 周)** |

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
