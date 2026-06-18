import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerRegisterTools(server: McpServer) {
  server.tool(
    'x64dbg_registers',
    'Read/write CPU registers including GPR, flags, and AVX-512',
    {
      action: z.discriminatedUnion("action", [
        z.object({ action: z.literal("get_all") }),
        z.object({ action: z.literal("get_specific"), registers: z.string().describe("Comma-separated register names") }),
        z.object({ action: z.literal("get_flags") }),
        z.object({ action: z.literal("get_avx512") }),
        z.object({ action: z.literal("set"), register: z.string(), value: z.string() })
      ])
    },
    async ({ action }) => {
      let data: any;
      switch (action.action) {
        case 'get_all': data = await httpClient.get('/api/registers/all'); break;
        case 'get_specific': data = await httpClient.get('/api/registers/get', { name: action.registers }); break;
        case 'get_flags': data = await httpClient.get('/api/registers/flags'); break;
        case 'get_avx512': data = await httpClient.get('/api/registers/avx512'); break;
        case 'set': data = await httpClient.post('/api/registers/set', { name: action.register, value: action.value }); break;
      }
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );
}