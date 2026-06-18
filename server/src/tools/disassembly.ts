import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerDisassemblyTools(server: McpServer) {
  server.tool(
    'x64dbg_disassembly',
    'Disassemble at an address, a whole function, or assemble new code',
    {
      action: z.discriminatedUnion("action", [
        z.object({ action: z.literal("at_address"), address: z.string().optional().default("cip"), count: z.coerce.string().optional().default("10") }),
        z.object({ action: z.literal("function"), address: z.string().optional().default("cip") }),
        z.object({ action: z.literal("info"), address: z.string().optional().default("cip") }),
        z.object({ action: z.literal("assemble"), address: z.string(), instruction: z.string(), write_to_memory: z.boolean().optional().default(false) })
      ])
    },
    async ({ action }) => {
      let data: any;
      switch (action.action) {
        case 'at_address': data = await httpClient.get('/api/disasm/at', { address: action.address, count: action.count }); break;
        case 'function': data = await httpClient.get('/api/disasm/function', { address: action.address }); break;
        case 'info': data = await httpClient.get('/api/disasm/basic', { address: action.address }); break;
        case 'assemble': data = await httpClient.post('/api/disasm/assemble', { address: action.address, instruction: action.instruction, write_to_memory: action.write_to_memory }); break;
      }
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );
}