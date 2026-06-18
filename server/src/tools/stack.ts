import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerStackTools(server: McpServer) {
  server.tool(
    'x64dbg_stack',
    'Stack operations: call stack, raw reads, SEH chain, return addresses, pointers',
    {
      action: z.discriminatedUnion("action", [
        z.object({ action: z.literal("get_call_stack") }),
        z.object({ action: z.literal("read"), size: z.coerce.string().optional().default("256") }),
        z.object({ action: z.literal("pointers") }),
        z.object({ action: z.literal("seh_chain") }),
        z.object({ action: z.literal("return_address") }),
        z.object({ action: z.literal("comment"), address: z.string() })
      ])
    },
    async ({ action }) => {
      let data: any;
      switch (action.action) {
        case 'get_call_stack': data = await httpClient.get('/api/stack/trace'); break;
        case 'read': data = await httpClient.get('/api/stack/read', { size: action.size }); break;
        case 'pointers': data = await httpClient.get('/api/stack/pointers'); break;
        case 'seh_chain': data = await httpClient.get('/api/stack/seh_chain'); break;
        case 'return_address': data = await httpClient.get('/api/stack/return_address'); break;
        case 'comment': data = await httpClient.get('/api/stack/comment', { address: action.address }); break;
      }
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );
}