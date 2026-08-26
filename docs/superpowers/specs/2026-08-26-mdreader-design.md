# MDReader — Design Spec

> **Date**: 2026-08-26
> **Topic**: Build MDReader, a cross-platform C++ Markdown reader/editor
> **Stack**: Qt 6 Widgets + cmark-gfm + QWebEngineView (Chromium)

## 1. Goals & Non-Goals

### Goals
- Horizontal split layout: source on left, rendered preview on right; user can drag the divider to resize.
- Live preview while typing (`~250ms` debounced), auto-reload of preview when the underlying file is updated outside the app, prompt-to-resolve when in-app edits conflict with an external change.
- Build and package as a Linux `.deb` and a Windows `.exe` installer from the same CMake project.
- UI/style reference: MacDown.app on macOS — multi-tab, outline (TOC) sidebar, info sidebar, multiple themes, syntax-highlighted code blocks, math (LaTeX), pasted images rendered inline.

### Non-Goals
- Cloud sync, accounts, telemetry.
- WYSIWYG editing (Markdown source is the source of truth).
- VCS integration beyond plain file open/save.
- Plugin system, scripting API.
- Mobile platforms.

## 2. Resolved Decisions (from brainstorming)

| # | Question | Decision |
|---|---|---|
| 1 | Feature scope | **Full MacDown-like set**: multi-tab, TOC + Info sidebars, themes, code highlighting, math, image paste (option D). |
| 2 | UI framework | **Qt 6 Widgets + QWebEngineView** (option A). |
| 3 | Markdown library | **cmark-gfm** (GitHub Flavored Markdown; option A). |
| 4 | External-file change behavior | **Smart**: live-debounced while typing; auto-reload when not dirty; modal "discard / keep / save-then-reload" when dirty. |
| 5 | Main layout | **C (all-panels)**: outline (left, hideable) + editor (draggable) + preview (draggable) + info (right, hideable). |
| 6 | Debounce timings | `250ms` for typing → render; `500ms` for file-watcher events. |
| 7 | Image assets | Saved next to the document as `.md.assets/<sha256>.<ext>`, so they move with the file. |
| 8 | Export | Both HTML and PDF (PDF uses `QWebEnginePage::printToPdf()`). |

## 3. Tech Stack

- C++17, CMake ≥ 3.21.
- Qt 6.5+ (`Widgets`, `WebEngineWidgets`).
- cmark-gfm — fetched via CMake `FetchContent`; vendored at `vendor/cmark-gfm/`.
- MathJax 3 (tex-mml-chtml) — vendored under `src/resources/vendor/mathjax/`.
- highlight.js 11 — vendored under `src/resources/vendor/highlight/`.
- Themes (CSS) — vendored under `src/resources/themes/`: `github.css`, `github-dark.css`, `dracula.css`, `solarized-light.css`, `solarized-dark.css`, `one-dark.css`.
- Build tools: CMake; packaging via CPack (DEB on Linux, NSIS on Windows). Windows builds must be native (cross-compiling Qt WebEngine is impractical).

## 4. Architecture

Lightweight MVC with a small services layer.

```
┌──────────────────────────────────────────────────────────────────┐
│ MainWindow  (menu, toolbar, QTabWidget host, QSettings I/O)      │
│   └── DocumentTab × N                                           │
│         ├── QSplitter A  [ OutlineView │ BodySplitter ]         │
│         │                          ├── QSplitter B              │
│         │                          │     ├── EditorView          │
│         │                          │     ├── QSplitter C        │
│         │                          │     │     ├── PreviewView   │
│         │                          │     │     └── InfoView      │
│   └── DocumentManager (open/save, current selection, session)    │
│                                                                  │
│ Services (no UI)                                                 │
│   ├── MarkdownRenderer   cmark-gfm → HTML + injected CSS/JS     │
│   ├── RenderCoordinator  250ms debounce & manual-mode gating     │
│   ├── FileWatcher        QFileSystemWatcher + 500ms debounce     │
│   ├── OutlineExtractor   markdown text → heading tree model     │
│   ├── WordCounter        words / chars / paragraphs / headings   │
│   └── ImageHandler       dropped/pasted image → sidecar assets  │
│                                                                  │
│ Resources                                                         │
│   └── resources.qrc       themes/, vendor/{highlight,mathjax}/,  │
│                            icons/                                │
└──────────────────────────────────────────────────────────────────┘
```

### 4.1 Module responsibilities

- **`Document`** — owns one file: path, current text, last-saved text, `dirty` flag, last-modified mtime. Detects identity by absolute path. Pure data; no Qt widgets.
- **`DocumentManager`** — list of `Document*`, current index, open/save/close, dirty-resolution on close, session persistence (QSettings).
- **`EditorView`** — `QPlainTextEdit` subclass: monospace font, line-number gutter (manual draw via `paintEvent`), handles image MIME.
- **`PreviewView`** — `QWebEngineView` wrapper with `setMarkdown(text)` that builds HTML payload (rendered body + theme CSS + vendor JS tags). Exposes `exportHtml(path)` (writes current rendered HTML) and `exportPdf(path)` (uses `QWebEnginePage::printToPdf()`).
- **`OutlineView`** — `QTreeView` reading from the extracted heading model; emits `headingActivated(lineNumber, id)`.
- **`InfoView`** — `QFormLayout`-based panel showing counts from `WordCounter`.
- **`DocumentTab`** — composes the four child views with the triple-nested splitter layout.
- **`MainWindow`** — toolbar actions, menu, theme menu, `QTabWidget`, splitter/state save & restore in `QSettings`.
- **`MarkdownRenderer`** — owns `cmark_parser`; converts text to HTML; adds `<style>` for active theme + `<script>` for highlight.js + MathJax; preserves raw `$$…$$` / `$…$` for MathJax to typeset.
- **`RenderCoordinator`** — owns a `QTimer`; on `EditorView::textChanged` restarts with 250 ms; manual mode disables until user triggers "Refresh".
- **`FileWatcher`** — one `QFileSystemWatcher`; each watched path has its own 500 ms `QTimer` to coalesce events into one `externalModified(docId, newBytes)`.
- **`OutlineExtractor`** — scans markdown text for `^(#{1,6})\s+(.+)$`, returns `QStandardItemModel`.
- **`WordCounter`** — counts on demand; pure function.
- **`ImageHandler`** — given `QByteArray`, computes SHA-256, writes next to the document at `<dir>/<basename>.assets/<hash>.<ext>`, returns the relative markdown image path.

### 4.2 Editor ↔ preview data flow

```
EditorView.textChanged
        │
        ▼
RenderCoordinator.requestRender(text)
        │  (250ms QTimer; single-shot)
        ▼
MarkdownRenderer.render(text, themeId) ──► HTML string + theme CSS injected
        │
        ▼
PreviewView.setHtml(html, baseUrl=QUrl("qrc:///"))
        │
        ▼
QWebEngineView page; on DOMContentLoaded highlight.js runs; on its completion,
MathJax.typesetPromise() runs.
```

### 4.3 External-change handling

```
FileWatcher.externalModified(docId, bytes)
        │
        ▼
DocumentManager.onExternalChange(docId, bytes)
        ├── if document.dirty == false:
        │     EditorView.setPlainText(bytes); RenderCoordinator.requestRender();
        │     statusBar.showMessage("Reloaded from disk")
        └── else:
              QMessageBox(three buttons, default = discard-and-reload)
                [ Discard my edits and reload ]
                  → drop in-memory content; reload disk bytes into editor; re-render.
                [ Keep my edits (ignore disk change) ]
                  → keep in-memory content; clear the file-watcher for this path
                    so we ignore further disk events until user saves.
                [ Save my edits, then reload ]
                  → write a backup of current in-memory content to
                    `<doc>.conflict-<UTC-timestamp>.md` next to the file, then
                    reload disk bytes into editor and re-render. The external
                    version becomes canonical; the user's edits are preserved
                    in the backup file.
```

### 4.4 Image paste flow

```
EditorView.insertFromMimeData() with image MIME
        │
        ▼
ImageHandler.handle(rawBytes, suggestedName)
        │
        ▼ (writes <docDir>/<basename>.assets/<sha256>.<ext>)
returns relative reference path
        │
        ▼
EditorView.insertText("![](" + ref + ")")      ← also re-renders immediately
```

## 5. Layout & Splitter Persistence

### 5.1 Splitter topology (per tab)

```
QSplitter A   (horizontal; persists QSettings key "layout/splitterA")
├── OutlineView                (hideable; min 120)
└── QSplitter B  (horizontal; persists "layout/splitterB")
    ├── EditorView                              (min 200)
    └── QSplitter C  (horizontal; persists "layout/splitterC")
        ├── PreviewView                         (min 200)
        └── InfoView                            (hideable; min 150)
```

### 5.2 Splitter behavior
- Cursor changes to `Qt::SplitHCursor` on hover over the handle.
- Dragging is clamped to per-child minimums; children cannot be zeroed by dragging.
- "☐ Outline" and "☐ Info" toolbar checkboxes hide/restore entire panels (not driven through the splitter handle) and remember their visible state.

### 5.3 Persistence
On `MainWindow::closeEvent` (and on tab-close / theme-change) we save:
- `MDReader/layout/splitterA/B/C` — `QByteArray` from `saveState()`.
- `MDReader/sidebar/outlineVisible`, `MDReader/sidebar/infoVisible` — bool.
- `MDReader/session/lastFiles` — `QStringList` of absolute paths of open tabs.
- `MDReader/session/currentIndex` — int.
- `MDReader/ui/theme` — `QString` (one of the theme names). Default on first run: `github`.
- `MDReader/ui/renderMode` — `"live"` (default) or `"manual"`.
- `MDReader/ui/showLineNumbers` — bool. Default on first run: `true`.

On startup `MainWindow` reads the keys in the order above and applies them (skipping any path that no longer exists, with a warning logged).

## 6. Build, Install, Packaging

### 6.1 Build (Linux)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCPACK_PACKAGE_VERSION=0.1.0
cmake --build . -j
cpack -G DEB
```
Outputs `MDReader-0.1.0-Linux.deb`. Runtime depends on system Qt 6.5+ packages (`libqt6core6`, `libqt6gui6`, `libqt6widgets6`, `libqt6webengine6`, `libqt6webenginewidgets6`).

### 6.2 Build (Windows, native on Windows host)
```bat
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j
windeployqt mdreader.exe
cpack -G NSIS -C Release
```
Outputs `MDReader-0.1.0-win64.exe`. NSIS produces a self-contained installer; `windeployqt` copies all Qt DLLs/plugins.

### 6.3 Install layout (Linux `.deb`)

| File | Path |
|---|---|
| Binary | `/usr/bin/mdreader` |
| `.desktop` | `/usr/share/applications/mdreader.desktop` |
| Icon (128×128) | `/usr/share/icons/hicolor/128x128/apps/mdreader.png` |
| MIME | `/usr/share/mime/packages/mdreader-mime.xml` |

`postinst` runs `update-mime-database` and `update-desktop-database`. Target distros: Ubuntu 22.04 LTS, Debian 12, Linux Mint 21+.

### 6.4 Install layout (Windows)
NSIS installer places `mdreader.exe` plus bundled Qt DLLs under `C:\Program Files\MDReader\` and registers a `.md` shell association.

### 6.5 Vendored resources
MathJax and highlight.js are checked in under `src/resources/vendor/`. They are loaded from `qrc:/vendor/...`. No runtime network access required.

## 7. Error Handling

| Scenario | Behavior |
|---|---|
| Open file fails (missing / no permission) | Status bar red error message; no tab created. |
| External file deleted | Status bar red message; tab keeps in-memory copy until user decides. |
| File >10 MB on open | Confirm dialog: `Continue?` (default = continue). Render throttle rises to 750 ms. |
| Image-paste write fails (disk full / read-only dir) | Red status bar message; markdown insertion rolled back. |
| `cmark-gfm` returns null / error | Catch in `MarkdownRenderer`; keep prior successful preview; status: "Render failed". |
| WebEngine preload (MathJax/highlight.js) broken at startup | Yellow banner: "Code highlighting or math rendering unavailable"; preview body still renders. |
| Splitter restored sizes sum to zero | Use defaults (Outline 200, Editor 50%, Preview 50%, Info 220). |
| Multiple tabs watching the same file path | Only first tab owns the watch; second receives updates via in-process signal. |
| Embedded image path no longer exists | Preview shows broken-image glyph; non-fatal. |

## 8. Testing

### 8.1 Unit (Qt Test, `tests/unit/`)
- `MarkdownRendererTest` — GFM table, GFM task list, fenced code with language, math `$$…$$` block preserved verbatim.
- `OutlineExtractorTest` — extracts H1–H6 in order from a fixture.
- `WordCounterTest` — mixed CJK/Latin counts.
- `FileWatcherDebounceTest` — five events within 300 ms produce one `externalModified`.
- `ImageHandlerTest` — identical bytes yield same hash; different bytes yield different files.

### 8.2 Integration (Qt Test, `tests/integration/`)
- `EditToPreviewTest` — `EditorView.setPlainText(text)` ⇒ wait for `RenderCoordinator` tick + `QWebEnginePage::loadFinished` ⇒ compare `page().mainFrame().toHtml()` against expected substring.
- `ThemeSwitchTest` — after `ThemeMenu::setTheme("dracula")`, preview HTML's `<style>` includes the Dracula class names.
- `ExternalReloadTest` — set dirty, write bytes externally, expect modal-displaying branch (verified via injected mock dialog).

### 8.3 Manual smoke (on both Linux and Windows)
- [ ] Launch from `.desktop` / Start menu.
- [ ] Open a `.md` file via GUI and via CLI (`mdreader foo.md`).
- [ ] Edit, save, reopen — round-trip identical.
- [ ] Edit in MDReader, simultaneously edit same file in Vim and save — modal appears with three buttons.
- [ ] Drag splitter — proportions persist across restart.
- [ ] Toggle outline / info — state persists.
- [ ] Switch theme — preview changes; editor unchanged.
- [ ] Paste an image — appears inline in preview.
- [ ] Render a fixture with `$$E=mc^2$$` and a fenced ` ```python ` block.
- [ ] Export to PDF and HTML.
- [ ] Screenshots archived in `docs/screenshots/`.

## 9. Acceptance Criteria

1. `cmake --build` produces a runnable binary on both Linux and Windows.
2. `cpack` produces a usable `.deb` (Linux) and `.exe` installer (Windows).
3. Opening a 1 MB Markdown file: first render < 500 ms; keystroke → preview update < 300 ms p95.
4. External edit, no in-app dirty → preview updates automatically.
5. External edit, in-app dirty → three-button modal with correct semantics and default.
6. Splitter sizes, theme, render mode, last-open files, and sidebar visibility all survive a restart.
7. Math `$$…$$` and fenced code blocks render correctly in preview after `highlight.js` and `MathJax` load.
8. Drag-and-drop / paste of an image writes to `<dir>/<basename>.assets/<sha256>.<ext>` and renders in preview.
9. `cpack -G DEB` and `cpack -G NSIS` each produce installable artifacts without manual intervention.
10. All unit + integration tests pass on CI.

## 10. Implementation Phases

| Phase | Content | Days |
|---|---|---|
| 0 | Repo scaffolding; `CMakeLists.txt`; Qt project skeleton; blank main window runs. | 1 |
| 1 | `Document` + `EditorView` + `PreviewView` + `MarkdownRenderer` + `RenderCoordinator` (250 ms debounce). | 3 |
| 2 | Triple-nested splitter in `DocumentTab`; `OutlineView` + `InfoView`; `QSettings` persistence; multi-tab; session restore. | 2 |
| 3 | Themes (5+) menu; live / manual render-mode toggle; outline↔editor sync (click + cursor); highlight.js; MathJax; image paste. | 3 |
| 4 | `FileWatcher` + 500 ms debounce + conflict modal + HTML export + PDF export. | 1 |
| 5 | CPack DEB + CPack NSIS + icon / `.desktop` / MIME registration. | 2 |
| 6 | Unit + integration tests + smoke on Linux & Windows + README. | 1 |
| **Total** | | **≈ 13 working days (≈ 2.5 weeks)** |

## 11. Repository Layout

```
MDReader/
├── CMakeLists.txt
├── cmake/                      # CPack helpers
├── docs/
│   ├── superpowers/specs/      # this file
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
├── vendor/cmark-gfm/           # fetched by CMake FetchContent
└── .gitignore
```

## 12. Open Questions for the User

_(none at spec-write time — all decisions captured above)_
