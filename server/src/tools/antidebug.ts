import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerAntidebugTools(server: McpServer) {
  server.tool('x64dbg_antidebug', 'PEB/TEB inspection, DEP status, hide debugger from anti-debug checks', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("peb"), pid: z.string().optional() }),
      z.object({ action: z.literal("teb"), tid: z.string().optional() }),
      z.object({ action: z.literal("dep") }),
      z.object({ action: z.literal("hide_debugger") })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'peb': {
        const params: Record<string, string> = {};
        if (action.pid) params.pid = action.pid;
        data = await httpClient.get('/api/antidebug/peb', params); break;
      }
      case 'teb': {
        const params: Record<string, string> = {};
        if (action.tid) params.tid = action.tid;
        data = await httpClient.get('/api/antidebug/teb', params); break;
      }
      case 'dep': data = await httpClient.get('/api/antidebug/dep_status'); break;
      case 'hide_debugger': data = await httpClient.post('/api/antidebug/hide_debugger'); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}