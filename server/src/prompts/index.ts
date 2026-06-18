import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';

export function registerAllPrompts(server: McpServer) {
  server.prompt(
    'debug_session',
    'Initialize a debugging session with environment assessment: check debugger state, identify the main module, and set up initial breakpoints.',
    { issue: z.string().optional().describe('Specific issue or goal for the session') },
    ({ issue }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Initialize a debugging session${issue ? ` for: ${issue}` : ''}. First, check the debugger state with x64dbg_debug (action: "state"). Then identify the main module with x64dbg_modules (action: "get_main"). Enumerate loaded modules with x64dbg_modules (action: "list"). Check current registers with x64dbg_registers (action: "get_all"). Get image info with x64dbg_command (action: "imageinfo") to understand the binary type. List any existing comments with x64dbg_command (action: "comment_list") and labels with x64dbg_command (action: "label_list") to see prior analysis. Provide a summary of the current debugging environment.`
        }
      }]
    })
  );

  server.prompt(
    'analyze_crash',
    'Systematic crash root cause analysis: examine the exception, call stack, register state, and surrounding code.',
    { address: z.string().optional().describe('Crash address if known') },
    ({ address }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Perform a crash analysis${address ? ` at address ${address}` : ''}. Get the debugger state with x64dbg_debug (action: "state"), examine the call stack with x64dbg_stack (action: "get_call_stack"), read CPU registers with x64dbg_registers (action: "get_all"), and disassemble around the crash site with x64dbg_disassembly (action: "at_address", address: "${address || 'cip'}", count: "30"). Get exception info with x64dbg_command (action: "exinfo") and list exception handlers with x64dbg_command (action: "exhandlers") to understand the exception context. If the crash involves an access violation, use x64dbg_command (action: "reffind", value: "${address || 'cip'}") to find what code references the crash address. Analyze the exception context and provide root cause analysis.`
        }
      }]
    })
  );

  server.prompt(
    'unpack_binary',
    'Automated unpacking workflow: detect packer, find OEP, and dump the unpacked binary with PE rebuild.',
    { module: z.string().optional().default('main').describe('Module to unpack') },
    ({ module }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Unpack the ${module} module. First, analyze it with x64dbg_advanced_dump (action: "analyze_module", module: "${module}"). If packed, detect the OEP using x64dbg_advanced_dump (action: "detect_oep", module: "${module}", strategy: "code_analysis"). Before running, suspend all threads with x64dbg_threads (action: "suspend_all") to freeze the unpacking stub. Use exception-swallowing stepping with x64dbg_debug (action: "sestep_over") to bypass anti-debug traps. If watchdog threads are detected, kill them with x64dbg_threads (action: "kill", id: "<tid>"). Then dump with x64dbg_advanced_dump (action: "dump_with_options", module: "${module}", output: "unpacked.bin", fix_oep: true, rebuild_pe: true, remove_integrity: true). Resume all threads with x64dbg_threads (action: "resume_all") after completion.`
        }
      }]
    })
  );

  server.prompt(
    'trace_function',
    'Set up function tracing with parameter monitoring and log output.',
    { name: z.string().describe('Function name or address to trace') },
    ({ name }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Trace function ${name}. Resolve the function address with x64dbg_symbols (action: "resolve", symbol: "${name}"). Get detailed mnemonic info with x64dbg_command (action: "mnemonichelp", mnemonic: "${name}") if it's an instruction. Set a breakpoint with x64dbg_breakpoints (action: "set_software", address: "${name}"). Configure trace logging with x64dbg_tracing (action: "log_setup", file: "trace.log", text: "{RAX} {RBX} {RCX} {RDX}"). Use x64dbg_command (action: "config", config_action: "set", key: "engine.breakpointint3", value: "1") to ensure breakpoints work. Start tracing with x64dbg_tracing (action: "into", max_steps: "1000"). Use x64dbg_command (action: "findasm", instruction: "call ${name}") to find all call sites. Monitor the execution and report findings.`
        }
      }]
    })
  );

  server.prompt(
    'find_vulnerability',
    'Security vulnerability scanning: search for dangerous API calls, buffer operations, and format string bugs.',
    { type: z.enum(['buffer-overflow', 'format-string', 'use-after-free', 'all']).optional().default('all').describe('Vulnerability type to scan for') },
    ({ type }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Scan the current binary for ${type === 'all' ? 'potential security vulnerabilities' : type + ' vulnerabilities'}. List all modules with x64dbg_modules (action: "list"), get the main module's imports with x64dbg_dumping (action: "imports", module: "main"). Use x64dbg_command (action: "modcallfind") to find inter-modular calls. Search for dangerous patterns with x64dbg_command (action: "findasm", instruction: "call [ebp+arg") to find stack buffer operations. Use x64dbg_command (action: "reffind", value: "0") to find null pointer dereferences. Use x64dbg_command (action: "refstr") to find all referenced strings and trace their usage. Analyze function cross-references and provide a vulnerability assessment report.`
        }
      }]
    })
  );

  server.prompt(
    'reverse_algorithm',
    'Algorithm identification and pseudocode generation: analyze a function to determine its algorithm and logic.',
    { address: z.string().describe('Address of the algorithm function') },
    ({ address }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Reverse engineer the algorithm at ${address}. Get function info with x64dbg_analysis (action: "function", query: "${address}"), disassemble the entire function with x64dbg_disassembly (action: "function", address: "${address}"), check for constants with x64dbg_database (action: "constants"), and identify cross-references with x64dbg_analysis (action: "xrefs_to", query: "${address}"). Use x64dbg_command (action: "reffind", value: "${address}") to find all callers. Check existing comments with x64dbg_command (action: "comment_list") and labels with x64dbg_command (action: "label_list") for prior analysis context. Use x64dbg_command (action: "mnemonichelp", mnemonic: "unknown") for any unfamiliar instructions. Determine the algorithm, explain each step, and provide pseudocode.`
        }
      }]
    })
  );

  server.prompt(
    'api_monitor',
    'Set up Windows API call monitoring with conditional breakpoints and logging.',
    { category: z.enum(['process', 'memory', 'file', 'registry', 'network', 'all']).optional().default('all').describe('API category to monitor') },
    ({ category }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Set up API monitoring for the ${category} category. Get the main module info with x64dbg_modules (action: "get_main"). Use x64dbg_command (action: "modcallfind") to find inter-modular calls. Set logging breakpoints on relevant Windows API functions. Configure x64dbg_tracing (action: "log_setup", file: "api_monitor.log", text: "{RAX} {RCX} {RDX} {R8} {R9}"). Use x64dbg_command (action: "gui_update_disable") to speed up monitoring. Start the target running with x64dbg_debug (action: "run"). If exceptions occur, use x64dbg_debug (action: "erun") to pass them to the debuggee and continue monitoring. After collecting data, use x64dbg_command (action: "gui_update_enable") to restore GUI. Analyze the API call patterns.`
        }
      }]
    })
  );

  server.prompt(
    'patch_code',
    'Guided binary patching workflow with backup and verification.',
    { goal: z.string().describe('What the patch should accomplish') },
    ({ goal }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Perform binary patching to: ${goal}. First, read the target bytes with x64dbg_memory (action: "read") to create a backup. Use x64dbg_command (action: "gui_update_disable") to speed up patching. Apply the patch with x64dbg_patches (action: "apply"). Verify with x64dbg_patches (action: "list") and re-read the patched bytes with x64dbg_memory (action: "read") to confirm. Use x64dbg_command (action: "gui_update_enable") to restore GUI. Test the patch and provide a summary of changes made.`
        }
      }]
    })
  );

  server.prompt(
    'hunt_strings',
    'Search for interesting strings with cross-reference analysis.',
    { pattern: z.string().optional().default('').describe('String pattern to search for') },
    ({ pattern }) => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: `Search for strings${pattern ? ` matching "${pattern}"` : ''} in the binary. Use x64dbg_search (action: "string", pattern: "${pattern}") to find strings. Use x64dbg_command (action: "refstr") to find all referenced strings in the module. For each interesting string found, check cross-references with x64dbg_analysis (action: "xrefs_to") and use x64dbg_command (action: "reffind", value: "<string_address>") to find what code references it. Disassemble the referencing code with x64dbg_disassembly (action: "function"). Compile a report of string usage and context.`
        }
      }]
    })
  );

  server.prompt(
    'compare_state',
    'Capture and compare execution states before and after a specific operation.',
    {},
    () => ({
      messages: [{
        role: 'user',
        content: {
          type: 'text',
          text: 'Capture and compare execution states. First capture a snapshot with x64dbg_context (action: "get_snapshot"). Execute the target operation. Then capture a second snapshot with x64dbg_context (action: "get_snapshot") again. Compare the two snapshots (you can use a manual approach by comparing register values, memory regions, and module state). Use x64dbg_command (action: "comment_list") and x64dbg_command (action: "label_list") to check if any annotations changed. Report all differences found.'
        }
      }]
    })
  );
}
