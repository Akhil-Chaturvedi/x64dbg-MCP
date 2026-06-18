import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerAdvancedDumpTools(server: McpServer) {
  server.tool('x64dbg_advanced_dump', 'Advanced module analysis and dumping: PE analysis with entropy, packer detection, OEP detection, configurable dump with options', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("analyze_module"), module: z.string().describe("Module name or base address") }),
      z.object({ action: z.literal("detect_oep"), module: z.string(), strategy: z.enum(['entropy', 'code_analysis', 'api_calls', 'tls']).optional().default('code_analysis') }),
      z.object({ action: z.literal("dumpable_regions"), module: z.string().optional() }),
      z.object({ action: z.literal("dump_with_options"), module: z.string(), output: z.string(), fix_oep: z.boolean().optional().default(true), rebuild_pe: z.boolean().optional().default(true), remove_integrity: z.boolean().optional().default(true), auto_detect_oep: z.boolean().optional().default(false), forced_oep: z.string().optional() }),
      z.object({ action: z.literal("dump_memory_region"), address: z.string(), size: z.coerce.string(), output: z.string(), as_raw: z.boolean().optional().default(false) })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'analyze_module':
        data = await httpClient.get('/api/dump/analyze', { module: action.module });
        break;
      case 'detect_oep':
        data = await httpClient.get('/api/dump/detect_oep', { module: action.module, strategy: action.strategy });
        break;
      case 'dumpable_regions': {
        const params: Record<string, string> = {};
        if (action.module) params.module = action.module;
        data = await httpClient.get('/api/dump/dumpable_regions', params);
        break;
      }
      case 'dump_with_options':
        data = await httpClient.post('/api/dump/with_options', {
          module: action.module, output: action.output,
          options: { fixOEP: action.fix_oep, rebuildPE: action.rebuild_pe, removeIntegrity: action.remove_integrity, autoDetectOEP: action.auto_detect_oep, forcedOEP: action.forced_oep }
        });
        break;
      case 'dump_memory_region':
        data = await httpClient.post('/api/dump/memory_region', {
          address: action.address, size: action.size, output: action.output, asRawBinary: action.as_raw
        });
        break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}