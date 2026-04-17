# wasm = WebAssembly
# Bash build script that runs Bison + Flex + Emscripten to generate and compile C compiler into wasm files (botscript.js + botscript.wasm) so the React app can use it in the browser

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SPECS_DIR="$ROOT_DIR/src/specs"
OUT_DIR="$ROOT_DIR/.wasm-build"
PUBLIC_WASM_DIR="$ROOT_DIR/public/wasm"

mkdir -p "$OUT_DIR" "$PUBLIC_WASM_DIR"

echo "[1/4] Generate parser with bison"
bison -d -o "$OUT_DIR/botscript.tab.c" "$SPECS_DIR/botscript.y"

echo "[2/4] Generate lexer with flex"
flex -o "$OUT_DIR/lex.yy.c" "$SPECS_DIR/botscript.l"

echo "[3/4] Compile to WebAssembly with emcc"
# We expose compile_json() to JavaScript
emcc \
  -O2 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME="createBotScriptModule" \
  -s EXPORTED_FUNCTIONS='["_compile_json","_free","_malloc"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","UTF8ToString"]' \
  -s ALLOW_MEMORY_GROWTH=1 \
  -I"$OUT_DIR" \
  "$OUT_DIR/botscript.tab.c" \
  "$OUT_DIR/lex.yy.c" \
  "$SPECS_DIR/wasm_runtime.c" \
  -o "$PUBLIC_WASM_DIR/botscript.js"

echo "[4/4] Done."
echo "Generated:"
echo "  - $PUBLIC_WASM_DIR/botscript.js"
echo "  - $PUBLIC_WASM_DIR/botscript.wasm"