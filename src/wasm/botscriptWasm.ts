// this file is a TypeScript “bridge” module that loads the generated wasm compiler (/wasm/botscript.js + .wasm) in the browser and exposes a function the React UI can call to compile BotScript code using the Flex/Bison WASM backend

export type WasmCompilerError = {
  type: "LEXICAL" | "SYNTACTIC" | "SEMANTIC";
  message: string;
  line: number;
  col: number;
};

export type WasmCompileResponse =
  | { ok: true; errors: [] }
  | { ok: false; errors: WasmCompilerError[] };

declare global {
  interface Window {
    createBotScriptModule?: () => Promise<any>;
  }
}

let modulePromise: Promise<any> | null = null;

async function loadScript(src: string) {
  await new Promise<void>((resolve, reject) => {
    const s = document.createElement("script");
    s.src = src;
    s.async = true;
    s.onload = () => resolve();
    s.onerror = () => reject(new Error(`Failed to load script: ${src}`));
    document.head.appendChild(s);
  });
}

export async function getBotScriptModule() {
  if (!modulePromise) {
    modulePromise = (async () => {
      await loadScript("/wasm/botscript.js");
      if (!window.createBotScriptModule) {
        throw new Error("createBotScriptModule not found on window");
      }
      return window.createBotScriptModule();
    })();
  }
  return modulePromise;
}

export async function compileInWasm(source: string): Promise<WasmCompileResponse> {
  const mod = await getBotScriptModule();

  const ptr = mod.ccall("compile_json", "number", ["string"], [source]);
  const json = mod.UTF8ToString(ptr);
  mod._free(ptr);

  return JSON.parse(json);
}