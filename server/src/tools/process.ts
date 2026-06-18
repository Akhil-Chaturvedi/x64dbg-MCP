import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerProcessTools(server: McpServer) {
  server.tool('x64dbg_process', 'Process info: basic, detailed, cmdline, elevated status, debugger version', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("basic") }),
      z.object({ action: z.literal("detailed") }),
      z.object({ action: z.literal("cmdline") }),
      z.object({ action: z.literal("elevated") }),
      z.object({ action: z.literal("dbversion") }),
      z.object({ action: z.literal("set_cmdline"), cmdline: z.string() })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'basic': data = await httpClient.get('/api/process/info'); break;
      case 'detailed': data = await httpClient.get('/api/process/details'); break;
      case 'cmdline': data = await httpClient.get('/api/process/cmdline'); break;
      case 'elevated': data = await httpClient.get('/api/process/elevated'); break;
      case 'dbversion': data = await httpClient.get('/api/process/dbversion'); break;
      case 'set_cmdline': data = await httpClient.post('/api/process/set_cmdline', { cmdline: action.cmdline }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerHandleTools(server: McpServer) {
  server.tool('x64dbg_handles', 'Handle inspection: list handles, TCP connections, windows, heaps', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("list_handles"), pid: z.string().optional() }),
      z.object({ action: z.literal("list_tcp") }),
      z.object({ action: z.literal("list_windows") }),
      z.object({ action: z.literal("list_heaps") }),
      z.object({ action: z.literal("get_name"), handle: z.string() }),
      z.object({ action: z.literal("close"), handle: z.string() })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'list_handles': {
        const params: Record<string, string> = {};
        if (action.pid) params.pid = action.pid;
        data = await httpClient.get('/api/handles/list', params); break;
      }
      case 'list_tcp': data = await httpClient.get('/api/handles/tcp'); break;
      case 'list_windows': data = await httpClient.get('/api/handles/windows'); break;
      case 'list_heaps': data = await httpClient.get('/api/handles/heaps'); break;
      case 'get_name': data = await httpClient.get('/api/handles/get', { handle: action.handle }); break;
      case 'close': data = await httpClient.post('/api/handles/close', { handle: action.handle }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerControlFlowTools(server: McpServer) {
  server.tool('x64dbg_control_flow', 'Control flow graph, branch analysis, loop detection, function management', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("cfg"), address: z.string() }),
      z.object({ action: z.literal("branch_dest"), address: z.string() }),
      z.object({ action: z.literal("is_jump_taken"), address: z.string() }),
      z.object({ action: z.literal("loops"), address: z.string() }),
      z.object({ action: z.literal("func_type"), address: z.string() }),
      z.object({ action: z.literal("add_function"), start: z.string(), end: z.string() }),
      z.object({ action: z.literal("delete_function"), address: z.string() })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'cfg': data = await httpClient.get('/api/cfg/function', { address: action.address }); break;
      case 'branch_dest': data = await httpClient.get('/api/cfg/branch_dest', { address: action.address }); break;
      case 'is_jump_taken': data = await httpClient.get('/api/cfg/is_jump_taken', { address: action.address }); break;
      case 'loops': data = await httpClient.get('/api/cfg/loops', { address: action.address }); break;
      case 'func_type': data = await httpClient.get('/api/cfg/func_type', { address: action.address }); break;
      case 'add_function': data = await httpClient.post('/api/cfg/add_function', { start: action.start, end: action.end }); break;
      case 'delete_function': data = await httpClient.post('/api/cfg/delete_function', { address: action.address }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerPatchTools(server: McpServer) {
  server.tool('x64dbg_patches', 'Apply byte patches, restore originals, list patches, or export patched module', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("list") }),
      z.object({ action: z.literal("apply"), address: z.string(), bytes: z.string(), restore: z.boolean().optional().default(false) }),
      z.object({ action: z.literal("restore"), address: z.string() }),
      z.object({ action: z.literal("export") })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'list': data = await httpClient.get('/api/patches/list'); break;
      case 'apply': data = await httpClient.post('/api/patches/apply', { address: action.address, bytes: action.bytes, restore: action.restore }); break;
      case 'restore': data = await httpClient.post('/api/patches/restore', { address: action.address }); break;
      case 'export': data = await httpClient.post('/api/patches/export'); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}