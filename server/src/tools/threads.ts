import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerThreadTools(server: McpServer) {
  server.tool('x64dbg_threads', 'Thread enumeration, TEB access, suspend/resume, switch', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("list") }), z.object({ action: z.literal("current") }), z.object({ action: z.literal("count") }),
      z.object({ action: z.literal("info"), tid: z.coerce.string() }), z.object({ action: z.literal("teb"), tid: z.coerce.string().optional() }),
      z.object({ action: z.literal("name"), tid: z.coerce.string() }), z.object({ action: z.literal("switch"), tid: z.coerce.number() }),
      z.object({ action: z.literal("suspend"), tid: z.coerce.number() }), z.object({ action: z.literal("resume"), tid: z.coerce.number() }),
        z.object({ action: z.literal("create"), entry: z.string().describe("Entry point address for new thread") }),
        z.object({ action: z.literal("kill"), id: z.coerce.string().describe("Thread ID to kill") }),
        z.object({ action: z.literal("resume_all") }),
        z.object({ action: z.literal("suspend_all") }),
        z.object({ action: z.literal("set_thread_name"), tid: z.coerce.number(), name: z.string() }),
        z.object({ action: z.literal("set_thread_priority"), tid: z.coerce.number(), priority: z.string() })
    ])
  }, async ({ action }) => {
    let data: any;
    switch (action.action) {
      case 'list': data = await httpClient.get('/api/threads/list'); break;
      case 'current': data = await httpClient.get('/api/threads/current'); break;
      case 'count': data = await httpClient.get('/api/threads/count'); break;
      case 'info': data = await httpClient.get('/api/threads/get', { id: action.tid }); break;
      case 'teb': data = await httpClient.get('/api/threads/teb', { tid: action.tid }); break;
      case 'name': data = await httpClient.get('/api/threads/name', { tid: action.tid }); break;
      case 'switch': data = await httpClient.post('/api/threads/switch', { id: action.tid }); break;
      case 'suspend': data = await httpClient.post('/api/threads/suspend', { id: action.tid }); break;
      case 'resume': data = await httpClient.post('/api/threads/resume', { id: action.tid }); break;
      case 'create': data = await httpClient.post('/api/threads/create', { entry: action.entry }); break;
      case 'kill': data = await httpClient.post('/api/threads/kill', { id: action.id }); break;
      case 'resume_all': data = await httpClient.post('/api/threads/resume_all'); break;
      case 'suspend_all': data = await httpClient.post('/api/threads/suspend_all'); break;
      case 'set_thread_name': data = await httpClient.post('/api/misc/set_thread_name', { tid: action.tid, name: action.name }); break;
      case 'set_thread_priority': data = await httpClient.post('/api/misc/set_thread_priority', { tid: action.tid, priority: action.priority }); break;
    }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}