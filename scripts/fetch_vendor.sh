#!/usr/bin/env bash
#
# fetch_vendor.sh — downloads 3rd-party JS assets (highlight.js, MathJax,
# Mermaid, …) into src/resources/vendor/ and registers them as Qt resources
# via src/CMakeLists.txt (qt_add_resources).
#
# Run once after cloning (or when bumping a vendor version). Each download is
# guarded by `[ -f ... ] ||` so re-running is a no-op. Network is required.
#
# Plan Task 22 added MathJax 3 (tex-mml-chtml bundle) so the preview can
# typeset $...$ and $$...$$ blocks without contacting a CDN at runtime.
#
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/src/resources/vendor/highlight"
mkdir -p "$ROOT/src/resources/vendor/mathjax"
mkdir -p "$ROOT/src/resources/vendor/mermaid"

[ -f "$ROOT/src/resources/vendor/highlight/highlight.min.js" ] || \
  curl -fLo "$ROOT/src/resources/vendor/highlight/highlight.min.js" \
       https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js

[ -f "$ROOT/src/resources/vendor/mathjax/tex-mml-chtml.js" ] || \
  curl -fLo "$ROOT/src/resources/vendor/mathjax/tex-mml-chtml.js" \
       https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js

# Mermaid 11.17.2. dist/mermaid.min.js is the self-contained UMD bundle (no
# dynamic imports), which is what lets the preview work offline — the ESM
# builds split diagram types into chunks that would need a network fetch.
# sha256 581ed7d74bd9048d0e3a91363927d72ef22942d7722546b27f7cc29e35390eb8
[ -f "$ROOT/src/resources/vendor/mermaid/mermaid.min.js" ] || \
  curl -fLo "$ROOT/src/resources/vendor/mermaid/mermaid.min.js" \
       https://cdn.jsdelivr.net/npm/mermaid@11.17.2/dist/mermaid.min.js
