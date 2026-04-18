#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SPECS_DIR="$ROOT_DIR/src/specs"
OUT_DIR="$ROOT_DIR/public/wasm"
EMSDK_DIR="${HOME}/emsdk"

mkdir -p "$OUT_DIR"

echo "[1/4] Resolve toolchain"

# Resolve bison/flex names (Windows winget vs msys2)
if command -v bison >/dev/null 2>&1; then
  BISON_CMD="bison"
elif command -v win_bison >/dev/null 2>&1; then
  BISON_CMD="win_bison"
else
  echo "Error: neither bison nor win_bison found in PATH"
  exit 1
fi

if command -v flex >/dev/null 2>&1; then
  FLEX_CMD="flex"
elif command -v win_flex >/dev/null 2>&1; then
  FLEX_CMD="win_flex"
else
  echo "Error: neither flex nor win_flex found in PATH"
  exit 1
fi

# Resolve emcc command safely (handles spaces in paths)
if command -v emcc >/dev/null 2>&1; then
  EMCC_CMD=(emcc)
elif [ -f "$EMSDK_DIR/upstream/emscripten/emcc.py" ]; then
  if [ -n "${EMSDK_PYTHON:-}" ] && [ -f "${EMSDK_PYTHON}" ]; then
    EMCC_CMD=("${EMSDK_PYTHON}" "$EMSDK_DIR/upstream/emscripten/emcc.py")
  elif command -v python3 >/dev/null 2>&1; then
    EMCC_CMD=(python3 "$EMSDK_DIR/upstream/emscripten/emcc.py")
  else
    echo "Error: emcc not found. Run: source ~/emsdk/emsdk_env.sh"
    exit 1
  fi
else
  echo "Error: emcc not found. Install/activate emsdk."
  exit 1
fi

echo "[2/4] Generate parser with $BISON_CMD"
"$BISON_CMD" -d -o "$SPECS_DIR/botscript.tab.c" "$SPECS_DIR/botscript.y"

echo "[3/4] Generate lexer with $FLEX_CMD"
"$FLEX_CMD" -o "$SPECS_DIR/lex.yy.c" "$SPECS_DIR/botscript.l"

echo "[4/4] Build WebAssembly module"
"${EMCC_CMD[@]}" \
  "$SPECS_DIR/botscript.tab.c" \
  "$SPECS_DIR/lex.yy.c" \
  "$SPECS_DIR/wasm_runtime.c" \
  -O2 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=0 \
  -s EXPORTED_FUNCTIONS='["_malloc","_free","_compile_json"]' \
  -s EXPORTED_RUNTIME_METHODS='["cwrap","UTF8ToString","stringToUTF8","lengthBytesUTF8"]' \
  -o "$OUT_DIR/botscript.js"