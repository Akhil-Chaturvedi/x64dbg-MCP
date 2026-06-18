import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerSearchTools(server: McpServer) {
  server.tool('x64dbg_search', 'Search for AOB patterns, strings, or symbol autocomplete', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("pattern"), pattern: z.string(), start: z.string().optional(), end: z.string().optional(), max_results: z.coerce.string().optional() }),
      z.object({ action: z.literal("string"), pattern: z.string().optional().default(""), module: z.string().optional() }),
      z.object({ action: z.literal("string_at"), address: z.string() }),
      z.object({ action: z.literal("symbol_auto_complete"), pattern: z.string() }),
      z.object({ action: z.literal("encode_type"), address: z.string() })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'pattern': data = await httpClient.post('/api/search/pattern', { pattern: action.pattern, address: action.start, size: action.end, max_results: action.max_results }); break;
      case 'string': data = await httpClient.post('/api/search/string', { text: action.pattern, module: action.module }); break;
      case 'string_at': data = await httpClient.get('/api/search/string_at', { address: action.address }); break;
      case 'symbol_auto_complete': data = await httpClient.post('/api/search/auto_complete', { search: action.pattern }); break;
      case 'encode_type': data = await httpClient.get('/api/search/encode_type', { address: action.address }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}