/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useRef } from 'react';
import { 
  Play, 
  RotateCcw, 
  Terminal, 
  Cpu, 
  BookOpen, 
  AlertCircle, 
  CheckCircle2,
  Code2,
  Bot,
  Settings,
  Github
} from 'lucide-react';
import { motion, AnimatePresence } from 'motion/react';
import { compileInWasm, WasmCompilerError } from './wasm/botscriptWasm';

type RobotState = {
  x: number;
  y: number;
  angle: number;
  penDown: boolean;
  color: string;
  history: { x: number; y: number; penDown: boolean; color: string }[];
};

type CompilerError = WasmCompilerError;

const DEFAULT_CODE = `// BotScript Example: Draw a Square
let size = 100;
let angle = 90;

color("#3b82f6");
penDown();

repeat 4 {
  forward(size);
  turn(angle);
}

penUp();
forward(50);
color("#ef4444");
penDown();
`;

export default function App() {
  const [code, setCode] = useState(DEFAULT_CODE);
  const [robotState, setRobotState] = useState<RobotState | null>(null);
  const [errors, setErrors] = useState<CompilerError[]>([]);
  const [logs, setLogs] = useState<string[]>([]);
  const [isRunning, setIsRunning] = useState(false);
  const [activeTab, setActiveTab] = useState<'editor' | 'docs' | 'compiler'>('editor');
  const [tokens, setTokens] = useState<any[]>([]);
  const [ast, setAst] = useState<any>(null);
  
  const canvasRef = useRef<HTMLCanvasElement>(null);

  const runCode = async () => {
    setIsRunning(true);
    setLogs(['Compiling with Flex/Bison (WASM)...']);
    setTokens([]);
    setAst(null);
    setErrors([]);

    try {
      const res = await compileInWasm(code);

      // Phase 3: always feed compiler panels from WASM output
      setTokens(res.tokens ?? []);
      setAst(res.ast ?? null);
      setErrors(res.errors ?? []);

      if (!res.ok) {
        setLogs(prev => [...prev, 'Compilation failed. Check errors below.']);
        setRobotState(null);
        return;
      }

      setLogs(prev => [
        ...prev,
        `Compilation successful (Flex/Bison). Tokens: ${res.tokens?.length ?? 0}`
      ]);

      // Phase 3: use WASM trace to build robot state/history
      if (res.trace && res.trace.length > 0) {
        const last = res.trace[res.trace.length - 1];

        setRobotState({
          x: last.x,
          y: last.y,
          angle: last.angle,
          penDown: last.penDown,
          color: last.color,
          history: res.trace.map((p: any) => ({
            x: p.x,
            y: p.y,
            penDown: p.penDown,
            color: p.color,
          })),
        });

        setLogs(prev => [...prev, `Execution trace loaded (${res.trace.length} points).`]);
      } else {
        setRobotState(null);
        setLogs(prev => [...prev, 'No execution trace returned.']);
      }
    } catch (err: any) {
      setLogs(prev => [...prev, `Runtime Error: ${err.message}`]);
      setRobotState(null);
    } finally {
      setIsRunning(false);
    }
  };

  const reset = () => {
    setRobotState(null);
    setErrors([]);
    setLogs(['System reset.']);
  };

  useEffect(() => {
    if (!canvasRef.current) return;
    const ctx = canvasRef.current.getContext('2d');
    if (!ctx) return;

    // Clear canvas
    ctx.clearRect(0, 0, canvasRef.current.width, canvasRef.current.height);
    
    // Draw grid
    ctx.strokeStyle = '#e2e8f0';
    ctx.lineWidth = 1;
    for (let i = 0; i <= 500; i += 50) {
      ctx.beginPath();
      ctx.moveTo(i, 0);
      ctx.lineTo(i, 500);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(0, i);
      ctx.lineTo(500, i);
      ctx.stroke();
    }

    if (robotState) {
      // Draw history
      ctx.lineWidth = 2;
      let lastX = robotState.history[0].x;
      let lastY = robotState.history[0].y;

      robotState.history.forEach((point, i) => {
        if (i === 0) return;
        if (point.penDown) {
          ctx.beginPath();
          ctx.strokeStyle = point.color;
          ctx.moveTo(lastX, lastY);
          ctx.lineTo(point.x, point.y);
          ctx.stroke();
        }
        lastX = point.x;
        lastY = point.y;
      });

      // Draw robot
      ctx.save();
      ctx.translate(robotState.x, robotState.y);
      ctx.rotate((robotState.angle * Math.PI) / 180);
      
      // Robot body
      ctx.fillStyle = '#1e293b';
      ctx.beginPath();
      ctx.moveTo(15, 0);
      ctx.lineTo(-10, -10);
      ctx.lineTo(-10, 10);
      ctx.closePath();
      ctx.fill();
      
      // Robot eye
      ctx.fillStyle = '#3b82f6';
      ctx.beginPath();
      ctx.arc(5, 0, 3, 0, Math.PI * 2);
      ctx.fill();
      
      ctx.restore();
    } else {
      // Draw initial robot
      ctx.save();
      ctx.translate(250, 250);
      ctx.fillStyle = '#94a3b8';
      ctx.beginPath();
      ctx.moveTo(15, 0);
      ctx.lineTo(-10, -10);
      ctx.lineTo(-10, 10);
      ctx.closePath();
      ctx.fill();
      ctx.restore();
    }
  }, [robotState]);

  return (
    <div className="min-h-screen bg-[#0f172a] text-slate-200 font-sans selection:bg-blue-500/30">
      {/* Header */}
      <header className="h-16 border-b border-slate-800 bg-slate-900/50 backdrop-blur-md flex items-center justify-between px-6 sticky top-0 z-50">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 bg-blue-600 rounded-xl flex items-center justify-center shadow-lg shadow-blue-500/20">
            <Bot className="text-white w-6 h-6" />
          </div>
          <div>
            <h1 className="font-bold text-lg tracking-tight text-white">BotScript IDE</h1>
            <p className="text-xs text-slate-400 font-mono">v1.0.4 // Compiler Lab</p>
          </div>
        </div>

        <div className="flex items-center gap-4">
          <div className="flex bg-slate-800 rounded-lg p-1">
            <button 
              onClick={() => setActiveTab('editor')}
              className={`px-4 py-1.5 rounded-md text-sm font-medium transition-all ${activeTab === 'editor' ? 'bg-slate-700 text-white shadow-sm' : 'text-slate-400 hover:text-slate-200'}`}
            >
              <Code2 className="w-4 h-4 inline-block mr-2" />
              Editor
            </button>
            <button 
              onClick={() => setActiveTab('compiler')}
              className={`px-4 py-1.5 rounded-md text-sm font-medium transition-all ${activeTab === 'compiler' ? 'bg-slate-700 text-white shadow-sm' : 'text-slate-400 hover:text-slate-200'}`}
            >
              <Cpu className="w-4 h-4 inline-block mr-2" />
              Compiler
            </button>
            <button 
              onClick={() => setActiveTab('docs')}
              className={`px-4 py-1.5 rounded-md text-sm font-medium transition-all ${activeTab === 'docs' ? 'bg-slate-700 text-white shadow-sm' : 'text-slate-400 hover:text-slate-200'}`}
            >
              <BookOpen className="w-4 h-4 inline-block mr-2" />
              Docs
            </button>
          </div>
          <div className="h-8 w-[1px] bg-slate-800 mx-2" />
          <button 
            onClick={reset}
            className="p-2 text-slate-400 hover:text-white hover:bg-slate-800 rounded-lg transition-colors"
            title="Reset Simulation"
          >
            <RotateCcw className="w-5 h-5" />
          </button>
          <button 
            onClick={runCode}
            disabled={isRunning}
            className="flex items-center gap-2 bg-blue-600 hover:bg-blue-500 disabled:opacity-50 text-white px-6 py-2 rounded-lg font-semibold transition-all shadow-lg shadow-blue-600/20 active:scale-95"
          >
            {isRunning ? <Cpu className="w-4 h-4 animate-spin" /> : <Play className="w-4 h-4" />}
            Execute
          </button>
        </div>
      </header>

      <main className="p-6 grid grid-cols-1 lg:grid-cols-12 gap-6 max-w-[1600px] mx-auto">
        {/* Left Column: Editor/Docs */}
        <div className="lg:col-span-7 flex flex-col gap-6">
          <div className="bg-slate-900 rounded-2xl border border-slate-800 overflow-hidden shadow-2xl flex flex-col h-[600px]">
            <div className="flex items-center justify-between px-4 py-3 border-b border-slate-800 bg-slate-900/50">
              <div className="flex items-center gap-2">
                <div className="flex gap-1.5">
                  <div className="w-3 h-3 rounded-full bg-red-500/20 border border-red-500/50" />
                  <div className="w-3 h-3 rounded-full bg-amber-500/20 border border-amber-500/50" />
                  <div className="w-3 h-3 rounded-full bg-emerald-500/20 border border-emerald-500/50" />
                </div>
                <span className="text-xs font-mono text-slate-500 ml-4">main.bs</span>
              </div>
              <div className="flex items-center gap-4 text-[10px] font-mono text-slate-500 uppercase tracking-widest">
                <span>UTF-8</span>
                <span>BotScript</span>
              </div>
            </div>
            
            <div className="flex-1 relative">
              <AnimatePresence mode="wait">
                {activeTab === 'editor' ? (
                  <motion.div 
                    key="editor"
                    initial={{ opacity: 0, x: -20 }}
                    animate={{ opacity: 1, x: 0 }}
                    exit={{ opacity: 0, x: 20 }}
                    className="h-full"
                  >
                    <textarea
                      value={code}
                      onChange={(e) => setCode(e.target.value)}
                      className="w-full h-full bg-transparent p-6 font-mono text-sm resize-none focus:outline-none text-blue-100/90 leading-relaxed"
                      spellCheck={false}
                      placeholder="Enter your BotScript code here..."
                    />
                  </motion.div>
                ) : activeTab === 'compiler' ? (
                  <motion.div 
                    key="compiler"
                    initial={{ opacity: 0, scale: 0.98 }}
                    animate={{ opacity: 1, scale: 1 }}
                    exit={{ opacity: 0, scale: 0.98 }}
                    className="h-full p-6 overflow-y-auto flex flex-col gap-6"
                  >
                    <div className="flex flex-col gap-3">
                      <h3 className="text-emerald-400 text-xs font-bold uppercase tracking-widest flex items-center gap-2">
                        <Terminal size={12} />
                        Flux de Jetons (yylex)
                      </h3>
                      <div className="bg-slate-950/50 rounded-xl border border-slate-800 p-4 font-mono text-[10px] h-48 overflow-y-auto">
                        {tokens.length > 0 ? (
                          <div className="space-y-1">
                            {tokens.map((t, i) => (
                              <div key={i} className="flex gap-4 border-b border-slate-900 pb-1">
                                <span className="text-slate-600 w-6">#{i}</span>
                                <span className="text-emerald-500 w-20">[{t.type}]</span>
                                <span className="text-blue-400">"{t.value}"</span>
                                <span className="text-slate-700 ml-auto">L{t.line}:C{t.col}</span>
                              </div>
                            ))}
                          </div>
                        ) : (
                          <div className="h-full flex items-center justify-center text-slate-700 italic">
                            Exécutez le code pour voir les jetons...
                          </div>
                        )}
                      </div>
                    </div>
                    <div className="flex flex-col gap-3 flex-1">
                      <h3 className="text-blue-400 text-xs font-bold uppercase tracking-widest flex items-center gap-2">
                        <Cpu size={12} />
                        Arbre de Syntaxe Abstraite (yyparse)
                      </h3>
                      <div className="bg-slate-950/50 rounded-xl border border-slate-800 p-4 font-mono text-[10px] flex-1 overflow-y-auto">
                        {ast ? (
                          <pre className="text-blue-300/60 leading-tight">
                            {JSON.stringify(ast, null, 2)}
                          </pre>
                        ) : (
                          <div className="h-full flex items-center justify-center text-slate-700 italic">
                            Exécutez le code pour voir l'AST...
                          </div>
                        )}
                      </div>
                    </div>
                  </motion.div>
                ) : (
                  <motion.div 
                    key="docs"
                    initial={{ opacity: 0, x: 20 }}
                    animate={{ opacity: 1, x: 0 }}
                    exit={{ opacity: 0, x: -20 }}
                    className="h-full p-8 overflow-y-auto prose prose-invert prose-slate max-w-none"
                  >
                    <h2 className="text-blue-400 text-xl font-bold mb-4">Spécifications (TP1/TP2)</h2>
                    <div className="grid grid-cols-1 gap-8">
                      <div>
                        <h3 className="text-white text-sm font-semibold uppercase tracking-wider mb-3">Analyse Lexicale (Flex)</h3>
                        <pre className="bg-slate-800 p-4 rounded-lg text-[10px] overflow-x-auto">
{`[0-9]+\\.[0-9]+      { yylval.reel = atof(yytext); return REEL; }
[0-9]+              { yylval.entier = atoi(yytext); return ENTIER; }
"let"               { return LET; }
"repeat"            { return REPEAT; }
"if"                { return IF; }
[a-zA-Z_][a-zA-Z0-9_]* { yylval.chaine = strdup(yytext); return IDENT; }`}
                        </pre>
                      </div>
                      <div>
                        <h3 className="text-white text-sm font-semibold uppercase tracking-wider mb-3">Analyse Syntaxique (Bison)</h3>
                        <pre className="bg-slate-800 p-4 rounded-lg text-[10px] overflow-x-auto">
{`program: statements ;
statements: statement | statements statement ;
statement: var_decl | assignment | repeat_loop | if_stmt | command ;
repeat_loop: REPEAT expression '{' statements '}' ;
expression: ENTIER | REEL | IDENT | expression '+' expression | ... ;`}
                        </pre>
                      </div>
                    </div>
                    <div className="mt-8 p-4 bg-slate-800/50 rounded-xl border border-slate-700">
                      <p className="text-xs text-slate-400 italic">
                        Ce compilateur est structuré selon les principes vus en TP : 
                        yylex() pour l'analyse lexicale et yyparse() pour l'analyse syntaxique.
                        Les fichiers sources complets (.l, .y) et les fichiers générés théoriques (.tab.c, .tab.h, lex.yy.c) sont disponibles dans le dossier src/specs/.
                      </p>
                    </div>
                  </motion.div>
                )}
              </AnimatePresence>
            </div>
          </div>

          {/* Console */}
          <div className="bg-slate-900 rounded-2xl border border-slate-800 overflow-hidden shadow-xl">
            <div className="flex items-center gap-2 px-4 py-2 bg-slate-800/50 border-b border-slate-800">
              <Terminal className="w-4 h-4 text-slate-400" />
              <span className="text-xs font-bold uppercase tracking-widest text-slate-400">System Console</span>
            </div>
            <div className="p-4 h-40 overflow-y-auto font-mono text-xs space-y-1">
              {logs.map((log, i) => (
                <div key={i} className={`flex gap-3 ${log.includes('failed') || log.includes('Error') ? 'text-red-400' : log.includes('successful') ? 'text-emerald-400' : 'text-slate-400'}`}>
                  <span className="opacity-30">[{new Date().toLocaleTimeString()}]</span>
                  <span>{log}</span>
                </div>
              ))}
              {errors.map((err, i) => (
                <div key={`err-${i}`} className="flex gap-3 text-red-400 bg-red-400/5 p-2 rounded border border-red-400/20">
                  <AlertCircle className="w-4 h-4 shrink-0" />
                  <span>
                    <strong>{err.type} Error:</strong> {err.message} (Line {err.line}, Col {err.col})
                  </span>
                </div>
              ))}
              {logs.length === 0 && errors.length === 0 && (
                <div className="text-slate-600 italic">Waiting for execution...</div>
              )}
            </div>
          </div>
        </div>

        {/* Right Column: Visualizer */}
        <div className="lg:col-span-5 flex flex-col gap-6">
          <div className="bg-slate-900 rounded-2xl border border-slate-800 overflow-hidden shadow-2xl flex flex-col">
            <div className="flex items-center justify-between px-4 py-3 border-b border-slate-800 bg-slate-900/50">
              <div className="flex items-center gap-2">
                <Bot className="w-4 h-4 text-blue-500" />
                <span className="text-xs font-bold uppercase tracking-widest text-slate-400">Visualizer // 2D Grid</span>
              </div>
              <div className="flex items-center gap-2">
                <div className={`w-2 h-2 rounded-full ${isRunning ? 'bg-blue-500 animate-pulse' : 'bg-slate-700'}`} />
                <span className="text-[10px] font-mono text-slate-500">{isRunning ? 'EXECUTING' : 'IDLE'}</span>
              </div>
            </div>
            
            <div className="p-6 bg-slate-950 flex items-center justify-center">
              <div className="relative group">
                <div className="absolute -inset-1 bg-gradient-to-r from-blue-600 to-indigo-600 rounded-xl blur opacity-20 group-hover:opacity-30 transition duration-1000"></div>
                <canvas 
                  ref={canvasRef}
                  width={500}
                  height={500}
                  className="relative bg-white rounded-lg shadow-inner cursor-crosshair"
                />
              </div>
            </div>

            <div className="p-6 border-t border-slate-800 grid grid-cols-2 gap-4">
              <div className="bg-slate-800/50 p-4 rounded-xl border border-slate-700/50">
                <span className="text-[10px] font-bold text-slate-500 uppercase tracking-wider block mb-1">Position</span>
                <div className="font-mono text-xl text-white">
                  {robotState ? `${Math.round(robotState.x)}, ${Math.round(robotState.y)}` : '250, 250'}
                </div>
              </div>
              <div className="bg-slate-800/50 p-4 rounded-xl border border-slate-700/50">
                <span className="text-[10px] font-bold text-slate-500 uppercase tracking-wider block mb-1">Heading</span>
                <div className="font-mono text-xl text-white">
                  {robotState ? `${Math.round(robotState.angle)}°` : '0°'}
                </div>
              </div>
            </div>
          </div>

          {/* Status Card */}
          <div className="bg-gradient-to-br from-blue-600/10 to-indigo-600/10 rounded-2xl border border-blue-500/20 p-6 flex items-center gap-5">
            <div className={`w-12 h-12 rounded-full flex items-center justify-center ${errors.length > 0 ? 'bg-red-500/20 text-red-400' : 'bg-emerald-500/20 text-emerald-400'}`}>
              {errors.length > 0 ? <AlertCircle className="w-6 h-6" /> : <CheckCircle2 className="w-6 h-6" />}
            </div>
            <div>
              <h3 className="font-bold text-white">Compiler Status</h3>
              <p className="text-sm text-slate-400">
                {errors.length > 0 
                  ? `${errors.length} issue(s) detected in source code.` 
                  : robotState ? 'Code executed successfully with zero errors.' : 'Ready for compilation.'}
              </p>
            </div>
          </div>
        </div>
      </main>

      {/* Footer */}
      <footer className="mt-12 border-t border-slate-800 p-8 text-center">
        <p className="text-slate-500 text-xs font-mono">
          &copy; 2025-2026 // 1 ISEOC - ISI // Mini-Projet Techniques de Compilation
        </p>
      </footer>
    </div>
  );
}