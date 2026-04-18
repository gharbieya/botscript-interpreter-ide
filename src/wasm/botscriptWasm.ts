export type WasmCompilerError = {
  type: "LEXICAL" | "SYNTACTIC" | "SEMANTIC";
  message: string;
  line: number;
  col: number;
};

export type WasmTokenType =
  | "ENTIER" | "REEL" | "IDENT" | "MOTCLE" | "OP_ARTHM" | "OP_REL"
  | "AFFECT" | "PUNCT" | "CHAINE" | "FIN" | "ERROR";

export interface WasmToken {
  type: WasmTokenType;
  value: any;
  line: number;
  col: number;
}

export interface WasmTracePoint {
  x: number;
  y: number;
  angle: number;
  penDown: boolean;
  color: string;
}

export type WasmAstNode = any;

export type WasmCompileResponse = {
  ok: boolean;
  errors: WasmCompilerError[];
  tokens: WasmToken[];
  ast: WasmAstNode | null;
  trace: WasmTracePoint[];
};

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
      if (!window.createBotScriptModule) throw new Error("createBotScriptModule not found on window");
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
  return JSON.parse(json) as WasmCompileResponse;
}