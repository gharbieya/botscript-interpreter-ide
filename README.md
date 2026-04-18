# BotScript IDE : User Manual

BotScript IDE is a web-based IDE for **BotScript**, a small domain-specific language (DSL) designed to control a virtual drawing robot. 
It shows tokens + AST in the UI and runs code with an interpreter.

## 1) Quick start (Windows / Linux)

### Prerequisites
- Node.js **18+**
- npm (comes with Node)

### Run the IDE
```bash
npm install
npm run dev
```
Then open `http://localhost:3000`.

> Note: The repository already includes a prebuilt WebAssembly compiler in `public/wasm/`, so you can run the IDE without installing Flex/Bison.

## 2) How to use the IDE

1. Open the **Editor** tab and write BotScript.
2. Click **Run**.
3. Check the **Compiler** tab to inspect:
   - the token stream (what the lexer produced)
   - the AST in JSON (what the parser built)
4. The **Canvas** shows the robot path (execution trace).

## 3) The BotScript DSL - detailed reference

### 3.1 Lexical rules

**Comments** (ignored):
- `// ...` single-line
- `# ...` single-line
- `/* ... */` multi-line

**Literals**:
- Numbers: integers and reals (e.g. `42`, `3.14`, `1e3`, `2.5E-2`)
- Strings: double-quoted strings with escapes (e.g. `"#3b82f6"`, `"hello\nworld"`)

**Identifiers**:
- `[a-zA-Z_][a-zA-Z0-9_]*`

**Keywords**:
- Declarations / control flow: `let`, `repeat`, `if`, `else`, `while`
- Robot commands: `forward`, `turn`, `color`, `penDown`, `penUp`

### 3.2 Program structure

A program is a sequence of statements. Statements end with `;` for declarations/assignments/commands. Blocks use `{ ... }`.

Supported statements:
- Variable declaration: `let name = expression;`
- Assignment: `name = expression;`
- Fixed loop: `repeat expression { statements }`
- While loop: `while (expression) { statements }`
- Conditional:
  - `if (expression) { statements }`
  - `if (expression) { statements } else { statements }`
- Command call:
  - `forward(expr);`, `turn(expr);`, `color(expr);`
  - `penDown();`, `penUp();`

### 3.3 Expressions and operators

BotScript uses **expressions everywhere** (loop counts, conditions, command arguments).

**Arithmetic**: `+`, `-`, `*`, `/`

**Relational**: `<`, `>`, `<=`, `>=`, `==`, `!=`

**Logical**:
- `&&` (and)
- `||` (or)
- `!`  (not)

**Precedence** (high → low):
1. Unary `!`
2. `*` `/`
3. `+` `-`
4. Relational: `<` `>` `<=` `>=` `==` `!=`
5. `&&`
6. `||`

> Implementation note: conditions are evaluated numerically (`0` is false, non-zero is true).

### 3.4 Built-in commands (robot API)

| Command | Signature | Effect |
|---|---|---|
| `forward` | `forward(dist);` | Move forward by `dist` units; draws if pen is down. |
| `turn` | `turn(angle);` | Rotate by `angle` degrees (clockwise). |
| `color` | `color("#RRGGBB");` | Set pen color (string). |
| `penDown` | `penDown();` | Start drawing. |
| `penUp` | `penUp();` | Stop drawing. |

## 4) Examples

### Example A — square
```c
let size = 100;
color("#3b82f6");
penDown();

repeat 4 {
  forward(size);
  turn(90);
}
```

### Example B — spiral
```c
let step = 5;
let i = 0;
color("#10b981");
penDown();

while (i < 60) {
  forward(step);
  turn(20);
  step = step + 2;
  i = i + 1;
}
```

### Example C — condition + logical operators
```c
let x = 0;
penDown();

while (x < 200) {
  if ((x < 80) || (x > 160)) {
    color("#ef4444");
  } else {
    color("#3b82f6");
  }

  forward(5);
  turn(10);
  x = x + 5;
}
```

## 5) Rebuilding the Flex/Bison compiler (WASM)

The WebAssembly compiler is generated from the specs in `src/specs/`:
- `botscript.l` (Flex lexer)
- `botscript.y` (Bison parser + AST actions)

The build script is `scripts/build-wasm.sh` and outputs to `public/wasm/`.

### Install prerequisites: Flex, Bison and `emcc`

You need:
- **Flex** (lexer generator)
- **Bison** (parser generator)
- **Emscripten** (provides `emcc`, compiles C to WebAssembly)

#### Linux / WSL (recommended)

Install Flex + Bison:
- Debian/Ubuntu:
  ```bash
  sudo apt update
  sudo apt install -y flex bison
  ```
- Fedora:
  ```bash
  sudo dnf install -y flex bison
  ```
- Arch:
  ```bash
  sudo pacman -S --noconfirm flex bison
  ```

Install Emscripten (emsdk) to get `emcc`:
```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

Verify:
```bash
flex --version
bison --version
emcc -v
```

> Tip: `scripts/build-wasm.sh` also looks for emsdk in `~/emsdk` by default.

#### Windows native (works, but more moving parts)

You need a Bash environment to run `scripts/build-wasm.sh`, either:
- **WSL** (recommended)
- **Git Bash**
- **MSYS2**

Flex/Bison options:
- If you use **MSYS2**, you can install them inside the MSYS2 shell:
  ```bash
  pacman -S --noconfirm flex bison
  ```
- If you use **WinFlexBison**, make sure `win_flex.exe` and `win_bison.exe` are on `PATH`.
  The build script auto-detects `bison`/`win_bison` and `flex`/`win_flex`.

Install Emscripten (emsdk) to get `emcc` (Git Bash / PowerShell):
```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
```

Then, in the same shell session, load the environment (path depends on your shell):
- Git Bash:
  ```bash
  source ./emsdk_env.sh
  ```
- PowerShell:
  ```powershell
  .\emsdk_env.ps1
  ```

Verify:
```bash
emcc -v
```

### Build + run (after installing prerequisites)

Build the WebAssembly compiler:
```bash
npm run compiler:wasm
```

Start the IDE:
```bash
npm run dev
```

Or do both in one command:
```bash
npm run dev:wasm
```

> On Windows, run these commands in WSL, MSYS2 or Git Bash if you want to rebuild the WASM compiler.
