/* Cppify UI bridge. All lexing, parsing, semantic checks, bytecode generation,
   and execution are performed by the C++ compiler compiled to WebAssembly. */
let compilerModule = null;
let compilerLoadPromise = null;
let lastBytecode = '';
let lastResult = null;

const examples = {
  hello: '// Hello World\nlet answer = 40 + 2;\nprint answer;\n',
  math: '// Basic arithmetic\nlet a = 42;\nlet b = 8;\nprint a + b;\nprint a - b;\nprint a * b;\nprint a / b;\n',
  precedence: '// Parentheses and operator precedence\nlet result = (10 + 5) * 2 - 8 / 2;\nprint result;\n',
  variables: '// Variables can be used in later expressions\nlet width = 12;\nlet height = 5;\nlet area = width * height;\nprint area;\n',
  error: '// Try a missing semicolon to see a compiler error\nlet answer = 10 + 20\nprint answer;\n'
};

function showToast(message, isError = false) {
  document.getElementById('cppify-toast')?.remove();
  const toast = document.createElement('div');
  toast.id = 'cppify-toast';
  toast.className = `fixed bottom-12 right-6 px-4 py-2.5 rounded-xl font-mono text-xs font-bold border shadow-2xl z-50 flex items-center space-x-2 ${isError ? 'bg-red-950 text-red-300 border-red-800' : 'bg-emerald-950 text-emerald-300 border-emerald-800'}`;
  const icon = document.createElement('i');
  icon.className = `fa-solid ${isError ? 'fa-circle-xmark' : 'fa-circle-check'}`;
  const text = document.createElement('span');
  text.textContent = message;
  toast.append(icon, text);
  document.body.appendChild(toast);
  window.setTimeout(() => toast.remove(), 2600);
}

async function loadCompiler() {
  if (compilerModule) return compilerModule;
  if (compilerLoadPromise) return compilerLoadPromise;
  compilerLoadPromise = (async () => {
    if (typeof window.createCppifyCompiler !== 'function') {
      throw new Error('The C++ WebAssembly compiler is missing. Build compiler.js and compiler.wasm using the Emscripten SDK, then commit both files. No JavaScript compiler fallback is used.');
    }
    const module = await window.createCppifyCompiler({
      locateFile: (path) => path.endsWith('.wasm') ? new URL(path, document.baseURI).toString() : path
    });
    if (typeof module.runTinyBasic !== 'function') throw new Error('The WebAssembly module loaded, but runTinyBasic() was not exported. Rebuild the compiler.');
    compilerModule = module;
    const badge = document.getElementById('editor-status-badge');
    badge.textContent = 'WASM Ready';
    badge.className = 'px-2 py-0.5 text-[10px] font-mono bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 rounded';
    document.querySelector('header .animate-pulse')?.classList.replace('bg-emerald-400', 'bg-emerald-400');
    return module;
  })();
  try { return await compilerLoadPromise; }
  catch (error) { compilerLoadPromise = null; throw error; }
}

function updateLineNumbers() {
  const editor = document.getElementById('source-editor');
  const box = document.getElementById('line-numbers');
  box.replaceChildren();
  const count = Math.max(5, editor.value.split('\n').length);
  for (let i = 1; i <= count; i++) {
    if (i > 1) box.appendChild(document.createElement('br'));
    box.appendChild(document.createTextNode(String(i)));
  }
}

function appendConsoleLog(message, isError = false) {
  const box = document.getElementById('cpu-terminal-out');
  const entry = document.createElement('div');
  entry.className = `p-2.5 rounded-lg border font-mono text-xs whitespace-pre-wrap break-words ${isError ? 'bg-red-950/40 border-red-800/80 text-red-300' : 'bg-slate-900/60 border-slate-800 text-slate-200'}`;
  entry.textContent = message;
  box.appendChild(entry);
  box.scrollTop = box.scrollHeight;
}

function renderTokens(tokens) {
  document.getElementById('token-count-badge').textContent = `${tokens.length} Tokens`;
  document.getElementById('token-json-out').textContent = JSON.stringify(tokens, null, 2);
  const container = document.getElementById('visual-tokens-container');
  container.replaceChildren();
  if (!tokens.length) {
    const empty = document.createElement('span');
    empty.className = 'text-xs text-slate-500 font-mono italic';
    empty.textContent = 'No tokens generated.';
    container.appendChild(empty);
    return;
  }
  for (const token of tokens) {
    const pill = document.createElement('span');
    const color = token.type === 'KEYWORD' ? 'bg-emerald-950 text-emerald-300 border-emerald-800' :
      token.type === 'NUMBER' ? 'bg-purple-950 text-purple-300 border-purple-800' :
      token.type === 'OPERATOR' ? 'bg-cyan-950 text-cyan-300 border-cyan-800' : 'bg-slate-900 text-slate-300 border-slate-700';
    pill.className = `px-2.5 py-1 rounded-md text-xs font-mono font-bold border shadow-sm ${color}`;
    pill.textContent = `${token.type}: ${JSON.stringify(token.value)} · ${token.line}:${token.column}`;
    container.appendChild(pill);
  }
}

function renderBytecode(bytecode, result) {
  lastBytecode = bytecode || '';
  document.getElementById('metric-bytes').textContent = `${new TextEncoder().encode(lastBytecode).length} Bytes`;
  document.getElementById('metric-variables').textContent = String(result.variableCount ?? 0);
  document.getElementById('metric-instructions').textContent = String(result.instructionCount ?? 0);
  document.getElementById('wasm-hex-matrix').textContent = lastBytecode || 'No bytecode generated.';
  const view = document.getElementById('disassembly-view');
  view.replaceChildren();
  if (!lastBytecode.trim()) {
    const empty = document.createElement('div');
    empty.className = 'text-slate-500 italic';
    empty.textContent = 'No bytecode generated yet.';
    view.appendChild(empty);
    return;
  }
  for (const line of lastBytecode.trimEnd().split('\n')) {
    const row = document.createElement('div');
    row.className = 'p-2.5 bg-slate-950 rounded-lg border border-slate-800/80 text-emerald-300 whitespace-pre-wrap break-words';
    row.textContent = line;
    view.appendChild(row);
  }
}

async function executeCompilePipeline() {
  const start = performance.now();
  const source = document.getElementById('source-editor').value;
  const status = document.getElementById('editor-status-badge');
  status.textContent = 'Compiling…';
  status.className = 'px-2 py-0.5 text-[10px] font-mono bg-cyan-500/10 text-cyan-300 border border-cyan-500/20 rounded';
  document.getElementById('cpu-terminal-out').replaceChildren();
  lastResult = null;
  try {
    const module = await loadCompiler();
    const result = JSON.parse(module.runTinyBasic(source));
    lastResult = result;
    renderTokens(result.tokens || []);
    renderBytecode(result.bytecode || '', result);
    if (!result.ok) {
      status.textContent = 'Error';
      status.className = 'px-2 py-0.5 text-[10px] font-mono bg-red-500/10 text-red-300 border border-red-500/20 rounded';
      appendConsoleLog(`[TinyBasic Compiler Error]\n${result.error}`, true);
      document.getElementById('execution-time-badge').textContent = `${(performance.now() - start).toFixed(2)} ms compile time`;
      showToast('Compilation Failed', true);
      return;
    }
    appendConsoleLog('[Cppify C++ Compiler / WebAssembly] Compilation successful.');
    appendConsoleLog('[Bytecode VM] Program output:');
    appendConsoleLog(result.output || '(Program completed with no output)');
    appendConsoleLog(`[Compiler] ${result.tokens.length} tokens · ${result.instructionCount} instructions · ${result.variableCount} variables`);
    status.textContent = 'Compiled';
    status.className = 'px-2 py-0.5 text-[10px] font-mono bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 rounded';
    document.getElementById('execution-time-badge').textContent = `${(performance.now() - start).toFixed(2)} ms total`;
    showToast('Compiled and executed in C++ WebAssembly');
  } catch (error) {
    status.textContent = 'WASM Missing';
    status.className = 'px-2 py-0.5 text-[10px] font-mono bg-red-500/10 text-red-300 border border-red-500/20 rounded';
    appendConsoleLog(`[Cppify Setup Error]\n${error.message}`, true);
    appendConsoleLog('Build the C++ compiler with ./build-wasm.sh (or build-wasm.bat on Windows), then serve this folder over HTTP.');
    document.getElementById('execution-time-badge').textContent = 'Compiler unavailable';
    showToast('C++ WebAssembly compiler unavailable', true);
  }
}

function loadSelectedTemplate() {
  const value = document.getElementById('template-select').value;
  document.getElementById('source-editor').value = examples[value] || examples.hello;
  updateLineNumbers();
  switchTab('cpu');
  executeCompilePipeline();
}
function clearEditor() {
  document.getElementById('source-editor').value = '';
  updateLineNumbers();
  showToast('Editor cleared');
}
function switchTab(name) {
  ['cpu', 'lexer', 'bytecode', 'disasm'].forEach((tab) => {
    document.getElementById(`tab-${tab}`).classList.toggle('active', tab === name);
    document.getElementById(`panel-${tab}`).classList.toggle('hidden', tab !== name);
  });
}
function clearConsole() {
  document.getElementById('cpu-terminal-out').replaceChildren();
  appendConsoleLog('[Cppify Engine] Console cleared.');
}
async function copyBytecode() {
  if (!lastBytecode) { showToast('Compile a program first!', true); return; }
  try {
    await navigator.clipboard.writeText(lastBytecode);
    showToast('Bytecode copied');
  } catch (_) { showToast('Clipboard access was blocked by the browser', true); }
}
async function downloadCompilerWasm() {
  try {
    const response = await fetch(new URL('compiler.wasm', document.baseURI));
    if (!response.ok) throw new Error(`Could not load compiler.wasm (HTTP ${response.status}). Build the compiler first.`);
    const blob = await response.blob();
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url; anchor.download = 'cppify-tinybasic-compiler.wasm';
    document.body.appendChild(anchor); anchor.click(); anchor.remove();
    URL.revokeObjectURL(url);
    showToast('Compiler WebAssembly downloaded');
  } catch (error) { showToast(error.message, true); }
}

window.addEventListener('DOMContentLoaded', () => {
  document.getElementById('template-select').addEventListener('change', loadSelectedTemplate);
  document.getElementById('source-editor').addEventListener('input', updateLineNumbers);
  document.getElementById('clear-editor-btn').addEventListener('click', clearEditor);
  document.getElementById('compile-btn').addEventListener('click', executeCompilePipeline);
  document.getElementById('clear-console-btn').addEventListener('click', clearConsole);
  document.getElementById('copy-hex-btn').addEventListener('click', copyBytecode);
  document.getElementById('download-wasm-btn').addEventListener('click', downloadCompilerWasm);
  document.querySelectorAll('[data-tab]').forEach((button) => button.addEventListener('click', () => switchTab(button.dataset.tab)));
  document.getElementById('source-editor').value = examples.hello;
  updateLineNumbers();
  loadCompiler().then(() => {
    appendConsoleLog('[Cppify Engine] C++ TinyBasic compiler loaded from WebAssembly.');
    executeCompilePipeline();
  }).catch((error) => {
    const badge = document.getElementById('editor-status-badge');
    badge.textContent = 'WASM Missing';
    badge.className = 'px-2 py-0.5 text-[10px] font-mono bg-red-500/10 text-red-300 border border-red-500/20 rounded';
    appendConsoleLog(`[Cppify Setup Error]\n${error.message}`, true);
    appendConsoleLog('The editor is ready, but compile/run is disabled until the C++ WebAssembly build is present.');
  });
});
