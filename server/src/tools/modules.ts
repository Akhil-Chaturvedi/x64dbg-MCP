import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerModuleTools(server: McpServer) {
  server.tool('x64dbg_modules', 'List modules or get info (base, section, party, imports, exports, main module)', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("list") }),
      z.object({ action: z.literal("get_info"), module: z.string() }),
      z.object({ action: z.literal("get_base"), module: z.string() }),
      z.object({ action: z.literal("get_section"), address: z.string().optional(), module: z.string().optional() }),
      z.object({ action: z.literal("get_party"), base: z.string() }),
      z.object({ action: z.literal("get_main") }),
      z.object({ action: z.literal("get_imports"), module: z.string() }),
      z.object({ action: z.literal("get_exports"), module: z.string() })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'list': data = await httpClient.get('/api/modules/list'); break;
      case 'get_info': data = await httpClient.get('/api/modules/get', { name: action.module }); break;
      case 'get_base': data = await httpClient.get('/api/modules/base', { name: action.module }); break;
      case 'get_section': {
        let address = action.address;
        if (!address && action.module) {
          const baseData = await httpClient.get('/api/modules/base', { name: action.module }) as any;
          address = baseData.base;
        }
        if (!address) throw new Error("Either 'address' or 'module' parameter is required");
        data = await httpClient.get('/api/modules/section', { address }); break;
      }
      case 'get_party': data = await httpClient.get('/api/modules/party', { base: action.base }); break;
      case 'get_main': data = await httpClient.get('/api/modules/main'); break;
      case 'get_imports': data = await httpClient.get('/api/modules/imports', { module: action.module }); break;
      case 'get_exports': data = await httpClient.get('/api/modules/exports', { module: action.module }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}