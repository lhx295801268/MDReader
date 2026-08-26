#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/src/resources/vendor/highlight"
mkdir -p "$ROOT/src/resources/vendor/mathjax"

[ -f "$ROOT/src/resources/vendor/highlight/highlight.min.js" ] || \
  curl -fLo "$ROOT/src/resources/vendor/highlight/highlight.min.js" \
       https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js
