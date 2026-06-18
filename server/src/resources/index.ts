import { McpServer, ResourceTemplate } from '@modelcontextprotocol/sdk/server/mcp.js';
import { httpClient } from '../http_client.js';

export function registerAllResources(server: McpServer) {
  // Direct resources — always available for the LLM to fetch on demand
  server.resource(
    'debugger_state',
    'x64dbg://state',
    async (uri) => ({
      contents: [{
        uri: uri.href,
        text: JSON.stringify(await httpClient.get('/api/debug/state'), null, 2),
        mimeType: 'application/json'
      }]
    })
  );

  server.resource('registers', 'x64dbg://registers', async (uri) => ({
    contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/registers/all'), null, 2), mimeType: 'application/json' }]
  }));

  server.resource('modules_summary', 'x64dbg://modules', async (uri) => ({
    contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/modules/list'), null, 2), mimeType: 'application/json' }]
  }));

  server.resource('threads_summary', 'x64dbg://threads', async (uri) => ({
    contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/threads/list'), null, 2), mimeType: 'application/json' }]
  }));

  server.resource('memory_map', 'x64dbg://memory_map', async (uri) => ({
    contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/memmap/list'), null, 2), mimeType: 'application/json' }]
  }));

  server.resource('breakpoints', 'x64dbg://breakpoints', async (uri) => ({
    contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/breakpoints/list'), null, 2), mimeType: 'application/json' }]
  }));

  server.resource('stack', 'x64dbg://stack', async (uri) => ({
    contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/stack/call_stack'), null, 2), mimeType: 'application/json' }]
  }));

  // Resource templates — parameterized URIs
  server.resource(
    'memory',
    new ResourceTemplate('x64dbg://memory/{address}/{size}', { list: undefined }),
    async (uri, { address, size }) => ({
      contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/memory/read', { address: String(address), size: String(size) }), null, 2), mimeType: 'application/json' }]
    })
  );

  server.resource(
    'disassembly',
    new ResourceTemplate('x64dbg://disassembly/{address}/{count}', { list: undefined }),
    async (uri, { address, count }) => ({
      contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/disasm/at', { address: String(address), count: String(count) }), null, 2), mimeType: 'application/json' }]
    })
  );

  server.resource(
    'module_info',
    new ResourceTemplate('x64dbg://module/{name}', { list: undefined }),
    async (uri, { name }) => ({
      contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/modules/get', { name: String(name) }), null, 2), mimeType: 'application/json' }]
    })
  );

  server.resource(
    'symbol_resolve',
    new ResourceTemplate('x64dbg://symbol/{name}', { list: undefined }),
    async (uri, { name }) => ({
      contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/symbols/resolve', { symbol: String(name) }), null, 2), mimeType: 'application/json' }]
    })
  );

  server.resource(
    'function_analysis',
    new ResourceTemplate('x64dbg://function/{address}', { list: undefined }),
    async (uri, { address }) => ({
      contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/analysis/function', { address: String(address) }), null, 2), mimeType: 'application/json' }]
    })
  );

  server.resource(
    'module_exports',
    new ResourceTemplate('x64dbg://export/{module}', { list: undefined }),
    async (uri, { module: mod }) => ({
      contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/dump/exports', { module: String(mod) }), null, 2), mimeType: 'application/json' }]
    })
  );

  server.resource(
    'module_imports',
    new ResourceTemplate('x64dbg://import/{module}', { list: undefined }),
    async (uri, { module: mod }) => ({
      contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/dump/imports', { module: String(mod) }), null, 2), mimeType: 'application/json' }]
    })
  );

  server.resource(
    'stack_trace',
    'x64dbg://stack_trace',
    async (uri) => ({
      contents: [{ uri: uri.href, text: JSON.stringify(await httpClient.get('/api/stack/call_stack'), null, 2), mimeType: 'application/json' }]
    })
  );
}