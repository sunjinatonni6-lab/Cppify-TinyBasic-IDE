#!/usr/bin/env bash
set -euo pipefail
if ! command -v emcc >/dev/null 2>&1; then
  echo "Emscripten (emcc) was not found. Install/activate the Emscripten SDK first." >&2
  echo "https://emscripten.org/docs/getting_started/downloads.html" >&2
  exit 1
fi
emcc main.cpp -std=c++17 -O2 -Wall -Wextra \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createCppifyCompiler \
  -s ENVIRONMENT=web \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s FILESYSTEM=0 \
  -s NO_EXIT_RUNTIME=1 \
  --bind \
  -o compiler.js
printf '\nBuild complete: compiler.js and compiler.wasm generated.\n'
printf 'Commit both generated files before deploying the static site to Vercel.\n'
