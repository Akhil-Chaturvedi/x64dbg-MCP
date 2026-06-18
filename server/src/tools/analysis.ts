import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerAnalysisTools(server: McpServer) {
  server.tool('x64dbg_analysis', 'Get function info, xrefs, basic blocks or source location for an address. Also supports function listing and detail.', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("function"), query: z.string().optional().default('cip') }),
      z.object({ action: z.literal("xrefs_to"), query: z.string().optional().default('cip') }),
      z.object({ action: z.literal("xrefs_from"), query: z.string().optional().default('cip') }),
      z.object({ action: z.literal("basic_blocks"), query: z.string().optional().default('cip') }),
      z.object({ action: z.literal("source"), query: z.string().optional().default('cip') }),
      z.object({ action: z.literal("mnemonic_brief"), query: z.string().describe('Mnemonic to look up') }),
      z.object({ action: z.literal("list_functions"), module: z.string().optional() }),
      z.object({ action: z.literal("get_function"), address: z.string() }),
        z.object({ action: z.literal("run_analysis"), module: z.string().optional(), type: z.enum(['full', 'control_flow', 'nukem']).optional().default('full') })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'function': data = await httpClient.get('/api/analysis/function', { address: action.query }); break;
      case 'xrefs_to': data = await httpClient.get('/api/analysis/xrefs_to', { address: action.query }); break;
      case 'xrefs_from': data = await httpClient.get('/api/analysis/xrefs_from', { address: action.query }); break;
      case 'basic_blocks': data = await httpClient.get('/api/analysis/basic_blocks', { address: action.query }); break;
      case 'source': data = await httpClient.get('/api/analysis/source', { address: action.query }); break;
      case 'mnemonic_brief': data = await httpClient.get('/api/analysis/mnemonic_brief', { mnemonic: action.query }); break;
      case 'list_functions': data = await httpClient.get('/api/analysis/functions', { module: action.module }); break;
      case 'get_function': data = await httpClient.get('/api/analysis/function_detail', { address: action.address }); break;
        case 'run_analysis': data = await httpClient.post('/api/analysis/run_analysis', { module: action.module, type: action.type }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerDatabaseTools(server: McpServer) {
  server.tool('x64dbg_database', 'List known constants, error codes, defined structs, or search strings in a module', {
    action: z.enum(['constants', 'error_codes', 'structs', 'strings']),
    module: z.string().optional()
  }, async ({ action, module }) => {
    let data: any;
    switch (action) {
      case 'constants': data = await httpClient.get('/api/analysis/constants'); break;
      case 'error_codes': data = await httpClient.get('/api/analysis/error_codes'); break;
      case 'structs': data = await httpClient.get('/api/analysis/structs'); break;
      case 'strings':
        if (!module) throw new Error("module is required for strings action");
        data = await httpClient.get('/api/analysis/strings', { module }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerAddressConvertTools(server: McpServer) {
  server.tool('x64dbg_address_convert', 'Convert Virtual Address (VA) to File Offset, or File Offset to VA', {
    action: z.enum(['va_to_file', 'file_to_va']),
    address: z.string().optional(),
    module: z.string().optional(),
    offset: z.string().optional()
  }, async ({ action, address, module, offset }) => {
    let data: any;
    if (action === 'va_to_file') {
      if (!address) throw new Error("address required");
      data = await httpClient.get('/api/analysis/va_to_file', { address });
    } else {
      if (!module || !offset) throw new Error("module and offset required");
      data = await httpClient.get('/api/analysis/file_to_va', { module, offset });
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerWatchdogTool(server: McpServer) {
  server.tool('x64dbg_watchdog', 'Check if a watch expression watchdog has been triggered', {
    id: z.coerce.string().optional().default('0')
  }, async ({ id }) => {
    const data = await httpClient.get('/api/analysis/watch', { id });
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerCompoundAnalysisTool(server: McpServer) {
  server.tool('x64dbg_analyze_module', 'Run multiple analysis passes on a module in a single call. Returns functions, imports, exports, sections, strings, and xrefs in one response.', {
    module: z.string().optional().default('main').describe('Module name or base address'),
    passes: z.array(z.enum(['functions', 'imports', 'exports', 'sections', 'strings', 'xrefs'])).optional().describe('Analysis passes to run. Default: all passes.')
  }, async ({ module, passes }) => {
    const data = await httpClient.post('/api/analysis/compound', { module, passes: passes || ['functions', 'imports', 'exports', 'sections', 'strings', 'xrefs'] });
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}

export function registerEventSubscriptionTool(server: McpServer) {
  server.tool('x64dbg_events', 'Subscribe or unsubscribe from debugger events (breakpoint hits, exceptions, module loads). Lets the LLM set up event-driven workflows.', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("subscribe"), event: z.enum(['breakpoint', 'exception', 'module_load']), address: z.string().optional().describe('Address for breakpoint event'), code: z.string().optional().describe('Exception code for exception event'), name: z.string().optional().describe('Module name for module_load event'), condition: z.string().optional().describe('Optional break condition expression') }),
      z.object({ action: z.literal("unsubscribe"), event: z.enum(['breakpoint', 'exception', 'module_load']), address: z.string().optional().describe('Address of breakpoint to remove'), code: z.string().optional().describe('Exception code to remove'), name: z.string().optional().describe('Module name to remove') })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'subscribe':
        data = await httpClient.post('/api/events/subscribe', { event: action.event, address: action.address, code: action.code, name: action.name, condition: action.condition });
        break;
      case 'unsubscribe':
        data = await httpClient.post('/api/events/unsubscribe', { event: action.event, address: action.address, code: action.code, name: action.name });
        break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}