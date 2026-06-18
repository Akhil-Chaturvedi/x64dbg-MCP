import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerContextTools(server: McpServer) {
  server.tool('x64dbg_context', 'Capture debugger state snapshots for execution state analysis. Use x64dbg_snapshot_diff compare to compare two captured snapshots.', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("get_snapshot") }),
      z.object({ action: z.literal("get_basic") })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'get_snapshot': data = await httpClient.get('/api/context/snapshot'); break;
      case 'get_basic': data = await httpClient.get('/api/context/basic'); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerFunctionTools(server: McpServer) {
  server.tool('x64dbg_function', 'List all recognized functions and get function boundaries at an address', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("list"), module: z.string().optional() }),
      z.object({ action: z.literal("get"), address: z.string() })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'list': {
        const params: Record<string, string> = {};
        if (action.module) params.module = action.module;
        data = await httpClient.get('/api/analysis/functions', params); break;
      }
      case 'get': data = await httpClient.get('/api/analysis/function_detail', { address: action.address }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerScriptTools(server: McpServer) {
  server.tool('x64dbg_script', 'Execute x64dbg scripts, batch commands, multi-step script engine, conditional workflows, and retrieve last command result. This is the unified scripting tool — prefer it over x64dbg_command for script-like workflows.', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("execute"), command: z.string().describe("Single x64dbg command string") }),
      z.object({ action: z.literal("execute_batch"), commands: z.array(z.string()).describe("Array of commands to execute sequentially") }),
      z.object({ action: z.literal("script_engine"), steps: z.array(z.string()).describe('Multi-step script engine: array of x64dbg commands executed sequentially. Returns results for all steps.') }),
      z.object({ action: z.literal("workflow"), steps: z.array(z.any()).describe('Conditional multi-step workflow. Each step is either {command: "..."} for direct execution, or {condition: "...", then: ["cmd1", "cmd2"]} for conditional branching.') }),
      z.object({ action: z.literal("get_last_result") })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'execute': data = await httpClient.post('/api/command/exec', { command: action.command }); break;
      case 'execute_batch': data = await httpClient.post('/api/command/execute_batch', { commands: action.commands }); break;
      case 'script_engine': data = await httpClient.post('/api/script/engine', { steps: action.steps }); break;
      case 'workflow': data = await httpClient.post('/api/workflow/execute', { steps: action.steps }); break;
      case 'get_last_result': data = await httpClient.get('/api/command/last_result'); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerSystemTools(server: McpServer) {
  server.tool('x64dbg_system', 'System information: server info, ping, list methods, and permission settings', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("info") }),
      z.object({ action: z.literal("ping") }),
      z.object({ action: z.literal("methods") }),
      z.object({ action: z.literal("permissions") })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'info': data = await httpClient.get('/api/health'); break;
      case 'ping': data = await httpClient.get('/api/health'); break;
      case 'methods': data = await httpClient.get('/api/system/methods'); break;
      case 'permissions': data = await httpClient.get('/api/system/permissions'); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerAssemblerTools(server: McpServer) {
  server.tool('x64dbg_assembler', 'Assemble an instruction at a given address, with optional write to memory', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("assemble"), address: z.string(), instruction: z.string(), write_to_memory: z.boolean().optional().default(false) })
    ])
  }, async ({ action }) => {
    let data: any;
    data = await httpClient.post('/api/disasm/assemble', { address: action.address, instruction: action.instruction, write_to_memory: action.write_to_memory });
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerBookmarkTools(server: McpServer) {
  server.tool('x64dbg_bookmark', 'Manage bookmarks: set, delete, or list bookmarks at addresses', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("set"), address: z.string() }),
      z.object({ action: z.literal("delete"), address: z.string() }),
      z.object({ action: z.literal("list") })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'set': data = await httpClient.post('/api/bookmarks/set', { address: action.address }); break;
      case 'delete': data = await httpClient.post('/api/bookmarks/delete', { address: action.address }); break;
      case 'list': data = await httpClient.get('/api/bookmarks/list'); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerSnapshotDiffTools(server: McpServer) {
  server.tool('x64dbg_snapshot_diff', 'Capture named state snapshots and compare them to detect changes in registers, memory, and breakpoints. Essential for patching workflows.', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("capture"), label: z.string().optional().describe('Optional label for the snapshot') }),
      z.object({ action: z.literal("compare"), snapshot1: z.any().describe('First snapshot JSON object'), snapshot2: z.any().describe('Second snapshot JSON object') })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'capture':
        data = await httpClient.post('/api/context/snapshot_capture', { label: action.label });
        break;
      case 'compare':
        data = await httpClient.post('/api/context/snapshot_compare', { snapshot1: action.snapshot1, snapshot2: action.snapshot2 });
        break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerAutoPatchTools(server: McpServer) {
  server.tool('x64dbg_auto_patch', 'Apply common patch patterns automatically: NOP, JMP, conditional jump inversion, RET, set byte. Handles byte-level details so the LLM can work at the goal level.', {
    type: z.enum(['nop', 'nop_range', 'jmp', 'je', 'jne', 'set_byte', 'ret', 'ret_0', 'invert_jz', 'invert_jnz']).describe('Type of patch to apply'),
    address: z.string().describe('Address to patch (hex string or expression)'),
    target: z.string().optional().describe('Target address for jump patches'),
    size: z.coerce.string().optional().describe('Number of bytes for nop_range'),
    value: z.coerce.string().optional().describe('Byte value for set_byte (hex without 0x)')
  }, async ({ type, address, target, size, value }) => {
    const data = await httpClient.post('/api/patches/auto', { type, address, target, size, value });
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerBatchAnnotationTools(server: McpServer) {
  server.tool('x64dbg_batch_annotate', 'Set multiple labels, comments, or bookmarks in a single call. Annotate entire functions without N sequential calls.', {
    annotations: z.array(z.object({
      type: z.enum(['label', 'comment', 'bookmark']).describe('Type of annotation'),
      address: z.string().describe('Address to annotate'),
      text: z.string().optional().describe('Text for label or comment (not needed for bookmark)')
    })).describe('Array of annotations to apply')
  }, async ({ annotations }) => {
    const data = await httpClient.post('/api/annotations/batch', { annotations });
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}