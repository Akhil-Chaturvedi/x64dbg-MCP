import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerDebugTools(server: McpServer) {
  server.tool(
    'x64dbg_debug',
    'Core debugger control: run, pause, step, restart, or get state. Also supports init (load executable), attach, and detach.',
    {
      action: z.discriminatedUnion("action", [
        z.object({ action: z.literal("run") }),
        z.object({ action: z.literal("pause") }),
        z.object({ action: z.literal("force_pause") }),
        z.object({ action: z.literal("step_into") }),
        z.object({ action: z.literal("step_over") }),
        z.object({ action: z.literal("step_out") }),
        z.object({ action: z.literal("stop_debug") }),
        z.object({ action: z.literal("restart_debug") }),
        z.object({
          action: z.literal("run_to_address"),
          address: z.string().describe("Target address to run to")
        }),
        z.object({
          action: z.literal("state"),
          include_health: z.boolean().optional().describe("Also check plugin health/version")
        }),
        z.object({
          action: z.literal("init"),
          path: z.string().optional().describe("Path to executable to load"),
          arguments: z.string().optional().describe("Command line arguments"),
          current_dir: z.string().optional().describe("Working directory")
        }),
        z.object({
          action: z.literal("attach"),
          pid: z.number().describe("Process ID to attach to")
        }),
        z.object({
          action: z.literal("detach")
        }),
        z.object({
          action: z.literal("undo")
        }),
        z.object({
          action: z.literal("run_to_user_code")
        }),
        z.object({
          action: z.literal("erun")
        }),
        z.object({
          action: z.literal("estep_into")
        }),
        z.object({
          action: z.literal("estep_over")
        }),
        z.object({
          action: z.literal("estep_out")
        }),
        z.object({
          action: z.literal("serun")
        }),
        z.object({
          action: z.literal("sestep_into")
        }),
        z.object({
          action: z.literal("sestep_over")
        }),
        z.object({
          action: z.literal("step_system")
        }),
        z.object({
          action: z.literal("step_user")
        }),
        z.object({
          action: z.literal("continue"),
          status: z.string().optional().default("HandleException").describe("Continue status: HandleException or ContinueExecution")
        })
      ])
    },
    async ({ action }) => {
      let endpoint = '';
      let payload: any = undefined;

      switch(action.action) {
        case 'run': endpoint = '/api/debug/run'; break;
        case 'pause': endpoint = '/api/debug/pause'; break;
        case 'force_pause': endpoint = '/api/debug/force_pause'; break;
        case 'step_into': endpoint = '/api/debug/step_into'; break;
        case 'step_over': endpoint = '/api/debug/step_over'; break;
        case 'step_out': endpoint = '/api/debug/step_out'; break;
        case 'stop_debug': endpoint = '/api/debug/stop'; break;
        case 'restart_debug': endpoint = '/api/debug/restart'; break;
        case 'run_to_address':
          endpoint = '/api/debug/run_to';
          payload = { address: action.address };
          break;
        case 'state':
          const stateData = await httpClient.get('/api/debug/state');
          let result: any = { state: stateData };
          if (action.include_health) {
            try {
              result.health = await httpClient.get('/api/health');
            } catch (e) {
              result.health = { error: "Health check failed" };
            }
          }
          return { content: [{ type: 'text', text: JSON.stringify(result, null, 2) }] };
        case 'init':
          endpoint = '/api/debug/init';
          payload = { path: action.path, arguments: action.arguments, current_dir: action.current_dir };
          break;
        case 'attach':
          endpoint = '/api/debug/attach';
          payload = { pid: action.pid };
          break;
        case 'detach':
          endpoint = '/api/debug/detach';
          break;
        case 'undo':
          endpoint = '/api/debug/undo';
          break;
        case 'run_to_user_code':
          endpoint = '/api/debug/run_to_user_code';
          break;
        case 'erun':
          endpoint = '/api/debug/erun';
          break;
        case 'estep_into':
          endpoint = '/api/debug/estep_into';
          break;
        case 'estep_over':
          endpoint = '/api/debug/estep_over';
          break;
        case 'estep_out':
          endpoint = '/api/debug/estep_out';
          break;
        case 'serun':
          endpoint = '/api/debug/serun';
          break;
        case 'sestep_into':
          endpoint = '/api/debug/sestep_into';
          break;
        case 'sestep_over':
          endpoint = '/api/debug/sestep_over';
          break;
        case 'step_system':
          endpoint = '/api/debug/step_system';
          break;
        case 'step_user':
          endpoint = '/api/debug/step_user';
          break;
        case 'continue':
          endpoint = '/api/debug/continue';
          payload = { status: action.status };
          break;
      }

      const data = await httpClient.post(endpoint, payload);
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );
}