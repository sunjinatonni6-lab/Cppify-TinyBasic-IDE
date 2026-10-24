
@echo off
where em++ >nul 2>nul
if errorlevel 1 (
  echo ERROR: Emscripten em++ was not found.
  exit /b 1
)

em++ main.cpp -std=c++17 -O2 -Wall -Wextra -s MODULARIZE=1 -s EXPORT_NAME=createCppifyCompiler -s ENVIRONMENT=web -s ALLOW_MEMORY_GROWTH=1 -s FILESYSTEM=0 -s NO_EXIT_RUNTIME=1 --bind -o compiler.js

if errorlevel 1 (
  echo ERROR: WebAssembly build failed.
  exit /b 1
)

if not exist compiler.js (
  echo ERROR: compiler.js was not generated.
  exit /b 1
)

if not exist compiler.wasm (
  echo ERROR: compiler.wasm was not generated.
  exit /b 1
)

echo SUCCESS: compiler.js and compiler.wasm generated.
