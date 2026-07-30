import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerMemoryTools(server: McpServer) {
  // Memory snapshot tool (XR3)
  server.tool(
    'x64dbg_memory_snapshot',
    'Take a snapshot of a memory region for later comparison. Use x64dbg_memory_diff to compare against current memory.',
    {
      address: z.string().describe('Address of the memory region (hex string or expression)'),
      size: z.string().describe('Size of the memory region in hex (e.g. "0x20000")'),
      label: z.string().describe('Label to identify this snapshot (e.g. "before_open")')
    },
    async ({ address, size, label }) => {
      const data = await httpClient.post('/api/memory/snapshot', { address, size, label });
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );

  // Memory diff tool (XR3)
  server.tool(
    'x64dbg_memory_diff',
    'Compare a previously captured memory snapshot with current memory to find what changed. Helps identify decoded buffers.',
    {
      label: z.string().describe('Label of the snapshot to compare against (e.g. "before_open")'),
      address: z.string().describe('Address of the memory region (hex string or expression)'),
      size: z.string().describe('Size of the memory region in hex (e.g. "0x20000")')
    },
    async ({ label, address, size }) => {
      const data = await httpClient.post('/api/memory/diff', { label, address, size });
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );

  // Snapshot management tools
  server.tool(
    'x64dbg_memory_snapshot_list',
    'List all stored memory snapshots with their labels and sizes.',
    {},
    async () => {
      const data = await httpClient.get('/api/memory/snapshot_list');
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );

  server.tool(
    'x64dbg_memory_snapshot_delete',
    'Delete a stored memory snapshot by label.',
    {
      label: z.string().describe('Label of the snapshot to delete')
    },
    async ({ label }) => {
      const data = await httpClient.post('/api/memory/snapshot_delete', { label });
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );
  server.tool(
    'x64dbg_memory',
    'Core memory operations: read, write, info, allocate, free, protect, map',
    {
      action: z.discriminatedUnion("action", [
        z.object({ action: z.literal("read"), address: z.string().describe("Hex string or expression"), size: z.coerce.string().optional().default("256").describe("Size in bytes (decimal)") }),
        z.object({ action: z.literal("write"), address: z.string().describe("Hex string or expression"), bytes: z.string().describe("Hex bytes (e.g. '90 90')"), verify: z.boolean().optional().default(false) }),
        z.object({ action: z.literal("info"), address: z.string() }),
        z.object({ action: z.literal("is_valid"), address: z.string() }),
        z.object({ action: z.literal("is_code"), address: z.string() }),
        z.object({ action: z.literal("allocate"), size: z.coerce.string().optional().default("0x1000") }),
        z.object({ action: z.literal("free"), address: z.string() }),
        z.object({ action: z.literal("protect"), address: z.string(), size: z.coerce.string().optional().default("0x1000"), protection: z.string() }),
        z.object({ action: z.literal("map"), address: z.string().optional() }),
        z.object({ action: z.literal("update_map") }),
        z.object({ action: z.literal("fill"), address: z.string(), value: z.coerce.string().describe("Byte value (0-255)"), size: z.coerce.string().describe("Number of bytes to fill") }),
        z.object({ action: z.literal("memcpy"), dest: z.string(), src: z.string(), size: z.coerce.string() }),
        z.object({ action: z.literal("page_rights"), address: z.string() }),
        z.object({ action: z.literal("set_page_rights"), address: z.string(), rights: z.string().describe('Rights string: "ReadWrite", "ExecuteReadWrite", "ReadOnly", "NoAccess", "WriteCopy", "Execute", "ExecuteRead", "ExecuteWriteCopy". Prefix with G for PAGE_GUARD e.g. "GReadOnly"') })
      ])
    },
    async ({ action }) => {
      let endpoint = '';
      let payload: any = undefined;
      switch (action.action) {
        case 'read': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.get('/api/memory/read', { address: action.address, size: action.size }), null, 2) }] };
        case 'write': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.post('/api/memory/write', { address: action.address, bytes: action.bytes, verify: action.verify }), null, 2) }] };
        case 'info': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.get('/api/memory/page_info', { address: action.address }), null, 2) }] };
        case 'is_valid': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.get('/api/memory/is_valid', { address: action.address }), null, 2) }] };
        case 'is_code': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.get('/api/memory/is_code', { address: action.address }), null, 2) }] };
        case 'allocate': endpoint = '/api/memory/allocate'; payload = { size: action.size }; break;
        case 'free': endpoint = '/api/memory/free'; payload = { address: action.address }; break;
        case 'protect': endpoint = '/api/memory/protect'; payload = { address: action.address, size: action.size, protection: action.protection }; break;
        case 'map':
          if (action.address) return { content: [{ type: 'text', text: JSON.stringify(await httpClient.get('/api/memmap/at', { address: action.address }), null, 2) }] };
          else return { content: [{ type: 'text', text: JSON.stringify(await httpClient.get('/api/memmap/list'), null, 2) }] };
        case 'update_map': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.post('/api/memory/update_map'), null, 2) }] };
        case 'fill': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.post('/api/memory/fill', { address: action.address, value: action.value, size: action.size }), null, 2) }] };
        case 'memcpy': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.post('/api/memory/memcpy', { dest: action.dest, src: action.src, size: action.size }), null, 2) }] };
        case 'page_rights': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.get('/api/memory/page_rights', { address: action.address }), null, 2) }] };
        case 'set_page_rights': return { content: [{ type: 'text', text: JSON.stringify(await httpClient.post('/api/memory/set_page_rights', { address: action.address, rights: action.rights }), null, 2) }] };
      }
      if (endpoint) { const data = await httpClient.post(endpoint, payload); return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
      }
      return { content: [{ type: 'text', text: "{}" }] };
    }
  );
}