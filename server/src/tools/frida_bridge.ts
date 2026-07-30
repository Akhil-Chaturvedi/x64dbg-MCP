import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { execFile } from 'child_process';
import { promisify } from 'util';
import { httpClient } from '../http_client.js';

const execFileAsync = promisify(execFile);

// ──────────────────────────────────────────────
// Frida CLI resolution
// ──────────────────────────────────────────────
// On Windows the `frida` command is installed by `pip install frida-tools`
// and lives in the Python Scripts directory. We try multiple strategies.

const FRIDA_PATH = process.env.X64DBG_FRIDA_PATH ?? '';  // user override
const PYTHON_PATH = process.env.X64DBG_PYTHON_PATH ?? ''; // user override for python

/**
 * Resolve the frida CLI command. Returns { command, args_prefix } where
 * args_prefix is prepended before the actual frida arguments.
 */
function resolveFridaCommand(): { command: string; argsPrefix: string[] } {
  if (FRIDA_PATH) {
    return { command: FRIDA_PATH, argsPrefix: [] };
  }
  // Try plain "frida" first (most common on Linux/macOS and Windows PATH)
  return { command: 'frida', argsPrefix: [] };
}

/**
 * Run a Frida one-shot script against a process.
 * Uses `frida -p <pid> -e "<code>"` which attaches, runs, prints output, detaches.
 * Falls back to `python -m frida_tools.repl` if the bare `frida` command fails.
 */
async function runFridaScript(pid: number, javascriptCode: string, timeoutMs = 15000): Promise<string> {
  // Wrap user code so output is a single JSON line we can parse
  const wrappedCode = `
    (function() {
      try {
        var result = (function() { ${javascriptCode} })();
        console.log("___FRIDA_RESULT___" + JSON.stringify(result));
      } catch(e) {
        console.log("___FRIDA_ERROR___" + JSON.stringify({message: e.toString(), stack: e.stack}));
      }
    })();
  `;

  const { command, argsPrefix } = resolveFridaCommand();
  const args = [...argsPrefix, '-p', String(pid), '-e', wrappedCode];

  try {
    const { stdout } = await execFileAsync(command, args, {
      timeout: timeoutMs,
      maxBuffer: 10 * 1024 * 1024, // 10MB
      windowsHide: true,
    });
    return stdout;
  } catch (err: any) {
    // If frida command not found, try python fallback
    if (err.code === 'ENOENT' || (err.stderr && err.stderr.includes('not recognized'))) {
      const pyCmd = PYTHON_PATH || 'python';
      const pyArgs = ['-m', 'frida_tools.repl', '-p', String(pid), '-e', wrappedCode];
      const { stdout } = await execFileAsync(pyCmd, pyArgs, {
        timeout: timeoutMs,
        maxBuffer: 10 * 1024 * 1024,
        windowsHide: true,
      });
      return stdout;
    }
    throw err;
  }
}

/**
 * Parse Frida CLI output to extract the JSON result line.
 */
function parseFridaOutput(output: string): { success: boolean; result?: any; error?: string; raw: string } {
  const lines = output.split('\n');
  for (const line of lines) {
    const trimmed = line.trim();
    if (trimmed.startsWith('___FRIDA_RESULT___')) {
      const jsonStr = trimmed.substring('___FRIDA_RESULT___'.length);
      try {
        return { success: true, result: JSON.parse(jsonStr), raw: output };
      } catch {
        return { success: true, result: jsonStr, raw: output };
      }
    }
    if (trimmed.startsWith('___FRIDA_ERROR___')) {
      const jsonStr = trimmed.substring('___FRIDA_ERROR___'.length);
      try {
        const errObj = JSON.parse(jsonStr);
        return { success: false, error: errObj.message || jsonStr, raw: output };
      } catch {
        return { success: false, error: jsonStr, raw: output };
      }
    }
  }
  // No marker found — return raw output
  return { success: true, result: undefined, raw: output };
}

// ──────────────────────────────────────────────
// Backtrace JS template
// ──────────────────────────────────────────────

function buildBacktraceJs(depth: number, skipFrames: number): string {
  return `
    var bt = Thread.backtrace(this.context, Backtracer.ACCURATE);
    var addresses = bt.slice(${skipFrames}, ${skipFrames + depth}).map(function(addr) {
      var sym = DebugSymbol.fromAddress(addr);
      var mod = Process.findModuleByAddress(addr);
      return {
        address: "0x" + addr.toString(16),
        name: sym && sym.name ? sym.name : "",
        moduleName: mod ? mod.name : "",
        moduleBase: mod ? "0x" + mod.base.toString(16) : "",
        offset: mod ? "0x" + addr.sub(mod.base).toString(16) : ""
      };
    });
    return addresses;
  `;
}

// ──────────────────────────────────────────────
// MCP Tool Registration
// ──────────────────────────────────────────────

export function registerFridaBridgeTools(server: McpServer) {
  server.tool(
    'x64dbg_frida_bridge',
    'Bridge between x64dbg and Frida for combined static+dynamic analysis. Capture backtraces from a running process via Frida and optionally set breakpoints in x64dbg at those addresses.',
    {
      action: z.discriminatedUnion('action', [
        // ── Action 1: Capture a native backtrace from a process ──
        z.object({
          action: z.literal('capture_backtrace'),
          pid: z.number().describe('Target process ID to capture backtrace from'),
          depth: z.number().optional().default(20).describe('Maximum number of stack frames to capture'),
          skip_frames: z.number().optional().default(0).describe('Skip the top N frames (usually Frida/runtime code)'),
          timeout_ms: z.number().optional().default(15000).describe('Timeout in milliseconds for the Frida operation')
        }),
        // ── Action 2: Set breakpoints in x64dbg from a Frida backtrace ──
        z.object({
          action: z.literal('set_bp_from_backtrace'),
          pid: z.number().describe('Target process ID to capture backtrace from'),
          bp_type: z.enum(['software', 'hardware']).optional().default('software').describe('Type of breakpoint to set in x64dbg'),
          hw_type: z.enum(['r', 'w', 'x']).optional().default('x').describe('Hardware breakpoint type (if bp_type=hardware)'),
          hw_size: z.enum(['1', '2', '4', '8']).optional().default('1').describe('Hardware breakpoint size (if bp_type=hardware)'),
          depth: z.number().optional().default(20).describe('Maximum number of stack frames to capture'),
          skip_frames: z.number().optional().default(0).describe('Skip the top N frames (usually Frida/runtime code)'),
          filter_module: z.string().optional().describe('Only set breakpoints for addresses in this module (e.g. "envoy7.exe")'),
          timeout_ms: z.number().optional().default(15000).describe('Timeout in milliseconds for the Frida operation')
        }),
        // ── Action 3: Execute arbitrary Frida JavaScript ──
        z.object({
          action: z.literal('execute_frida'),
          pid: z.number().describe('Target process ID'),
          javascript_code: z.string().describe('Frida JavaScript code to execute (use `return` to return a value)'),
          timeout_ms: z.number().optional().default(15000).describe('Timeout in milliseconds')
        })
      ])
    },
    async ({ action }) => {
      switch (action.action) {
        // ───────────────────────────────────────
        // capture_backtrace
        // ───────────────────────────────────────
        case 'capture_backtrace': {
          const btJs = buildBacktraceJs(action.depth, action.skip_frames);
          let output: string;
          try {
            output = await runFridaScript(action.pid, btJs, action.timeout_ms);
          } catch (err: any) {
            return {
              content: [{
                type: 'text',
                text: JSON.stringify({
                  success: false,
                  error: `Frida execution failed: ${err.message}`,
                  stderr: err.stderr || '',
                  hint: 'Ensure frida-tools is installed (pip install frida-tools) and the process is not being exclusively debugged by another tool.'
                }, null, 2)
              }]
            };
          }
          const parsed = parseFridaOutput(output);
          return {
            content: [{
              type: 'text',
              text: JSON.stringify({
                success: parsed.success,
                pid: action.pid,
                frames: parsed.result || [],
                frame_count: Array.isArray(parsed.result) ? parsed.result.length : 0,
                error: parsed.error,
                raw_output: parsed.raw.substring(0, 2000) // truncated for readability
              }, null, 2)
            }]
          };
        }

        // ───────────────────────────────────────
        // set_bp_from_backtrace
        // ───────────────────────────────────────
        case 'set_bp_from_backtrace': {
          // Step 1: Capture the backtrace
          const btJs = buildBacktraceJs(action.depth, action.skip_frames);
          let output: string;
          try {
            output = await runFridaScript(action.pid, btJs, action.timeout_ms);
          } catch (err: any) {
            return {
              content: [{
                type: 'text',
                text: JSON.stringify({
                  success: false,
                  error: `Frida backtrace failed: ${err.message}`,
                  hint: 'Ensure frida-tools is installed and the process exists.'
                }, null, 2)
              }]
            };
          }
          const parsed = parseFridaOutput(output);
          if (!parsed.success || !Array.isArray(parsed.result)) {
            return {
              content: [{
                type: 'text',
                text: JSON.stringify({
                  success: false,
                  error: 'Failed to parse backtrace from Frida output',
                  raw_output: parsed.raw.substring(0, 2000)
                }, null, 2)
              }]
            };
          }

          // Step 2: Filter frames if filter_module is specified
          let frames = parsed.result;
          if (action.filter_module) {
            frames = frames.filter((f: any) =>
              f.moduleName && f.moduleName.toLowerCase().includes(action.filter_module!.toLowerCase())
            );
          }

          // Step 3: Set breakpoints in x64dbg for each address
          const results: any[] = [];
          let succeeded = 0;
          let failed = 0;

          for (const frame of frames) {
            const addr = frame.address;
            try {
              let bpPayload: any;
              if (action.bp_type === 'hardware') {
                bpPayload = {
                  address: addr,
                  bp_type: 'hardware',
                  hw_type: action.hw_type,
                  hw_size: action.hw_size,
                  name: `frida_bt_${frame.moduleName}+${frame.offset}`
                };
              } else {
                bpPayload = {
                  address: addr,
                  bp_type: 'software',
                  name: `frida_bt_${frame.moduleName}+${frame.offset}`
                };
              }
              const bpResult = await httpClient.post('/api/breakpoints/configure', bpPayload);
              results.push({ address: addr, module: frame.moduleName, offset: frame.offset, name: frame.name, success: true });
              succeeded++;
            } catch (err: any) {
              results.push({ address: addr, module: frame.moduleName, offset: frame.offset, name: frame.name, success: false, error: err.message });
              failed++;
            }
          }

          return {
            content: [{
              type: 'text',
              text: JSON.stringify({
                success: true,
                pid: action.pid,
                bp_type: action.bp_type,
                filter_module: action.filter_module || '(all)',
                total_frames: parsed.result.length,
                filtered_frames: frames.length,
                breakpoints_set: succeeded,
                breakpoints_failed: failed,
                results
              }, null, 2)
            }]
          };
        }

        // ───────────────────────────────────────
        // execute_frida
        // ───────────────────────────────────────
        case 'execute_frida': {
          let output: string;
          try {
            output = await runFridaScript(action.pid, action.javascript_code, action.timeout_ms);
          } catch (err: any) {
            return {
              content: [{
                type: 'text',
                text: JSON.stringify({
                  success: false,
                  error: `Frida execution failed: ${err.message}`,
                  stderr: err.stderr || '',
                  hint: 'Ensure frida-tools is installed (pip install frida-tools) and the process is accessible.'
                }, null, 2)
              }]
            };
          }
          const parsed = parseFridaOutput(output);
          return {
            content: [{
              type: 'text',
              text: JSON.stringify({
                success: parsed.success,
                pid: action.pid,
                result: parsed.result,
                error: parsed.error,
                raw_output: parsed.raw.substring(0, 5000) // truncated
              }, null, 2)
            }]
          };
        }
      }
    }
  );
}
