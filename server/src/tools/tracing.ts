import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerTracingTools(server: McpServer) {
  server.tool('x64dbg_tracing', 'Execution tracing: into, over, run, stop, animate, conditional, log setup, hitcount, type', {
    action: z.discriminatedUnion("action", [
      z.object({ action: z.literal("into"), condition: z.string().optional(), max_steps: z.coerce.string().optional(), log_text: z.string().optional() }),
      z.object({ action: z.literal("over"), condition: z.string().optional(), max_steps: z.coerce.string().optional(), log_text: z.string().optional() }),
      z.object({ action: z.literal("run"), party: z.string().optional().default("0") }),
      z.object({ action: z.literal("stop") }),
      z.object({ action: z.literal("status") }),
      z.object({ action: z.literal("animate"), command: z.string() }),
      z.object({ action: z.literal("conditional_run"), break_condition: z.string().optional(), log_text: z.string().optional(), log_condition: z.string().optional(), command_text: z.string().optional(), cmd_condition: z.string().optional(), type: z.enum(['into','over']).optional().default('into') }),
      z.object({ action: z.literal("log_setup"), file: z.string(), text: z.string().optional(), condition: z.string().optional() }),
      z.object({ action: z.literal("hitcount"), address: z.string() }),
      z.object({ action: z.literal("type"), address: z.string() }),
      z.object({ action: z.literal("set_type"), address: z.string(), type: z.number() })
    ])
  }, async ({ action }) => {
    let data: any;
    let endpoint = '';
    let payload: any = undefined;
    switch (action.action) {
      case 'into': endpoint = '/api/trace/into'; payload = { condition: action.condition, max_steps: action.max_steps, log_text: action.log_text }; break;
      case 'over': endpoint = '/api/trace/over'; payload = { condition: action.condition, max_steps: action.max_steps, log_text: action.log_text }; break;
      case 'run': endpoint = '/api/trace/run'; payload = { party: action.party }; break;
      case 'stop': endpoint = '/api/trace/stop'; break;
      case 'status': data = await httpClient.get('/api/trace/status'); return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
      case 'animate': endpoint = '/api/trace/animate'; payload = { command: action.command }; break;
      case 'conditional_run': endpoint = '/api/trace/conditional_run'; payload = { break_condition: action.break_condition, log_text: action.log_text, log_condition: action.log_condition, command_text: action.command_text, cmd_condition: action.cmd_condition, type: action.type }; break;
      case 'log_setup': endpoint = '/api/trace/log'; payload = { file: action.file, text: action.text, condition: action.condition }; break;
      case 'hitcount': data = await httpClient.get('/api/trace/record/hitcount', { address: action.address }); return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
      case 'type': data = await httpClient.get('/api/trace/record/type', { address: action.address }); return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
      case 'set_type': endpoint = '/api/trace/record/set_type'; payload = { address: action.address, type: action.type }; break;
    }
    if (endpoint) { data = await httpClient.post(endpoint, payload); return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] }; }
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}