#!/usr/bin/env bash
#
# fetch_vendor.sh — downloads 3rd-party JS assets (highlight.js, MathJax, …)
# into src/resources/vendor/ and registers them as Qt resources via
# src/CMakeLists.txt (qt_add_resources).
#
# Run once after cloning (or when bumping a vendor version). Each download is
# guarded by `[ -f ... ] ||` so re-running is a no-op. Network is required.
#
# Note: mathjax is intentionally not downloaded yet — it's vendored in
# Plan Task 22. Re-run this script after Task 22 lands.
#
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/src/resources/vendor/highlight"
mkdir -p "$ROOT/src/resources/vendor/mathjax"

[ -f "$ROOT/src/resources/vendor/highlight/highlight.min.js" ] || \
  curl -fLo "$ROOT/src/resources/vendor/highlight/highlight.min.js" \
       https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js
