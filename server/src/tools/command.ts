import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { httpClient } from '../http_client.js';

export function registerCommandTools(server: McpServer) {
  server.tool(
    'x64dbg_command',
    'Execute x64dbg commands, scripts, eval expressions, or manage debug session. Includes batch execution and last result retrieval.',
    {
      action: z.discriminatedUnion("action", [
        z.object({ action: z.literal("execute"), command: z.string().describe("x64dbg command string") }),
        z.object({ action: z.literal("script"), commands: z.array(z.string()).describe("Array of commands") }),
        z.object({ action: z.literal("evaluate"), expression: z.string() }),
        z.object({ action: z.literal("format"), format: z.string() }),
        z.object({ action: z.literal("set_init_script"), file: z.string().describe("Path to script file") }),
        z.object({ action: z.literal("get_init_script") }),
        z.object({ action: z.literal("get_hash") }),
        z.object({ action: z.literal("get_events") }),
        z.object({ action: z.literal("execute_batch"), commands: z.array(z.string()).describe("Array of commands to execute sequentially") }),
        z.object({ action: z.literal("last_result") }),
        z.object({ action: z.literal("loadlib"), dll_path: z.string().describe("Full path to DLL") }),
        z.object({ action: z.literal("graph"), address: z.string().optional().default("cip") }),
        z.object({ action: z.literal("gpa"), name: z.string().describe("Export name"), module: z.string().describe("DLL module name") }),
        z.object({ action: z.literal("script_exec"), file: z.string().describe("Path to script file to execute") }),
        z.object({ action: z.literal("database_save"), file: z.string().optional().describe("Optional file path to save database") }),
        z.object({ action: z.literal("database_load"), file: z.string().describe("File path to load database from") }),
        z.object({ action: z.literal("imageinfo"), module: z.string().optional().default("0").describe("Module name or base") }),
        z.object({ action: z.literal("symdownload"), module: z.string().optional().describe("Module name"), store: z.string().optional().describe("Symbol store URL") }),
        z.object({ action: z.literal("symunload"), module: z.string().optional().describe("Module name") }),
        z.object({ action: z.literal("exinfo") }),
        z.object({ action: z.literal("exhandlers") }),
        z.object({ action: z.literal("virtualmod"), address: z.string().describe("Base address"), size: z.coerce.string().describe("Size of memory range") }),
        z.object({ action: z.literal("findguid"), guid: z.string().describe("GUID string to find references to") }),
        z.object({ action: z.literal("modcallfind"), module: z.string().optional().describe("Module name") }),
        z.object({ action: z.literal("argument_add"), address: z.string(), name: z.string() }),
        z.object({ action: z.literal("argument_del"), address: z.string() }),
        z.object({ action: z.literal("argument_clear") }),
        z.object({ action: z.literal("argument_list") }),
        z.object({ action: z.literal("comment_clear") }),
        z.object({ action: z.literal("label_clear") }),
        z.object({ action: z.literal("function_clear") }),
        z.object({ action: z.literal("bookmark_clear") }),
        z.object({ action: z.literal("set_thread_name"), tid: z.string(), name: z.string() }),
        z.object({ action: z.literal("set_thread_priority"), tid: z.string(), priority: z.string() }),
        z.object({ action: z.literal("gui_update_disable") }),
        z.object({ action: z.literal("gui_update_enable") }),
        z.object({ action: z.literal("get_jit") }),
        z.object({ action: z.literal("set_jit"), debugger: z.string().describe("Path to debugger executable") }),
        z.object({ action: z.literal("get_jit_auto") }),
        z.object({ action: z.literal("set_jit_auto"), enabled: z.string().describe("'true' or 'false'") }),
        z.object({ action: z.literal("plugload"), path: z.string().describe("Full path to plugin DLL") }),
        z.object({ action: z.literal("plugunload"), name: z.string().describe("Plugin name") }),
        z.object({ action: z.literal("zzz"), ms: z.coerce.string().optional().default("1000").describe("Milliseconds to sleep") }),
        z.object({ action: z.literal("reffind"), value: z.string().describe("Value/address to find references to") }),
        z.object({ action: z.literal("refstr") }),
        z.object({ action: z.literal("findasm"), instruction: z.string().describe("Assembly instruction to find") }),
        z.object({ action: z.literal("config"), config_action: z.enum(['get', 'set']).optional().default('get').describe("'get' or 'set'"), key: z.string().optional().describe("Config key. Known working keys include: 'EngineCalculation', 'EngineSymbolic', 'EngineAnalysis', 'DisassemblerArguments', 'DisassemblerTabbedMnemonic', 'DisassemblerMemorySpaces', 'DisassemblerUppercase', 'DisassemblerOnlyCipAutoComments', 'DisassemblerAutoComments', 'ExceptionsIgnoredRange'. Note: not all keys are guaranteed to be recognized by the plugin. For arbitrary INI settings, prefer bridge_setting_get/bridge_setting_set."), value: z.string().optional().describe("Config value (for set)") }),
        z.object({ action: z.literal("mnemonichelp"), mnemonic: z.string().describe("Assembly mnemonic to look up") }),
        z.object({ action: z.literal("script_load"), file: z.string().describe("Path to script file to load") }),
        z.object({ action: z.literal("script_run") }),
        z.object({ action: z.literal("comment_list") }),
        z.object({ action: z.literal("label_list") }),
        z.object({ action: z.literal("bpgoto"), address: z.string().describe("Breakpoint address"), target: z.string().describe("Redirect target address") }),
        z.object({ action: z.literal("chd"), path: z.string().describe("New working directory path") }),
        z.object({ action: z.literal("memmapdump"), address: z.string().optional().default("cip").describe("Address to follow in memory map") }),
        z.object({ action: z.literal("sdump"), address: z.string().optional().default("csp").describe("Stack address to dump") }),
        z.object({ action: z.literal("set_freezestack"), freeze: z.string().optional().default("1").describe("'1' to freeze, '0' to unfreeze") }),
        z.object({ action: z.literal("database_clear") }),
        z.object({ action: z.literal("label_delete"), address: z.string().describe("Address of label to delete") }),
        z.object({ action: z.literal("variable_new"), name: z.string().describe("Variable name"), value: z.string().describe("Initial value") }),
        z.object({ action: z.literal("variable_delete"), name: z.string().describe("Variable name to delete") }),
        z.object({ action: z.literal("variable_list") }),
        z.object({ action: z.literal("traceexecute"), address: z.string().optional().default("cip").describe("Address to mark as traced") }),
        z.object({ action: z.literal("script_cmd"), command: z.string().describe("Command to execute in script context") }),
        z.object({ action: z.literal("script_dll"), path: z.string().describe("Path to script DLL") }),
        z.object({ action: z.literal("reffind_range"), start: z.string().describe("Start of value range"), end: z.string().describe("End of value range") }),
        z.object({ action: z.literal("set_max_results"), max_results: z.coerce.string().optional().default("1000").describe("Maximum search results") }),
        z.object({ action: z.literal("refinit") }),
        z.object({ action: z.literal("refadd"), address: z.string().describe("Address to add"), text: z.string().describe("Reference text") }),
        z.object({ action: z.literal("refget"), address: z.string().describe("Address to get reference for") }),
        z.object({ action: z.literal("hide_debugger") }),
        z.object({ action: z.literal("clear_log") }),
        z.object({ action: z.literal("disable_log") }),
        z.object({ action: z.literal("enable_log") }),
        z.object({ action: z.literal("get_reloc_size"), module: z.string().optional().default("0").describe("Module name or base") }),
        z.object({ action: z.literal("set_bp_options"), type: z.coerce.string().optional().default("0").describe("Breakpoint type: 0=INT3, 1=LONG_INT3, 2=UD2") }),
        z.object({ action: z.literal("enable_privilege"), privilege: z.string().optional().default("SeDebugPrivilege").describe("Privilege name") }),
        z.object({ action: z.literal("disable_privilege"), privilege: z.string().optional().default("SeDebugPrivilege").describe("Privilege name") }),
        z.object({ action: z.literal("get_privilege_state"), privilege: z.string().optional().default("SeDebugPrivilege").describe("Privilege name") }),
        z.object({ action: z.literal("fold_disassembly"), start: z.string().describe("Start address"), end: z.string().describe("End address") }),
        z.object({ action: z.literal("set_memory_range_bp"), address: z.string(), size: z.coerce.string() }),
        z.object({ action: z.literal("librarian_set_bp"), name: z.string().describe("DLL name") }),
        z.object({ action: z.literal("librarian_remove_bp"), name: z.string().describe("DLL name") }),
        z.object({ action: z.literal("librarian_enable_bp"), name: z.string().describe("DLL name") }),
        z.object({ action: z.literal("librarian_disable_bp"), name: z.string().describe("DLL name") }),
        z.object({ action: z.literal("get_bp_hitcount"), type: z.enum(['software', 'exception', 'hardware', 'librarian', 'memory']).optional().default('software'), address: z.string().optional() }),
        z.object({ action: z.literal("reset_bp_hitcount"), type: z.enum(['software', 'exception', 'hardware', 'librarian', 'memory']).optional().default('software'), address: z.string().optional() }),
        z.object({ action: z.literal("add_favourite_command"), command: z.string() }),
        z.object({ action: z.literal("add_favourite_tool"), path: z.string(), description: z.string() }),
        z.object({ action: z.literal("add_favourite_shortcut"), shortcut: z.string(), description: z.string() }),
        z.object({ action: z.literal("start_scylla") }),
        z.object({ action: z.literal("set_data_type"), type: z.enum(['ascii', 'byte', 'code', 'double', 'dword', 'float', 'fword', 'junk', 'longdouble', 'middle', 'mmword', 'oword', 'qword', 'tbyte', 'unicode', 'unknown', 'word', 'xmmword', 'ymmword']), address: z.string() }),
        z.object({ action: z.literal("add_struct"), name: z.string() }),
        z.object({ action: z.literal("add_union"), name: z.string() }),
        z.object({ action: z.literal("add_type_alias"), name: z.string(), type: z.string() }),
        z.object({ action: z.literal("add_function_type"), name: z.string(), return_type: z.string(), args: z.string() }),
        z.object({ action: z.literal("add_arg"), name: z.string(), type: z.string() }),
        z.object({ action: z.literal("add_member"), name: z.string(), type: z.string() }),
        z.object({ action: z.literal("append_arg"), name: z.string(), type: z.string() }),
        z.object({ action: z.literal("append_member"), name: z.string(), type: z.string() }),
        z.object({ action: z.literal("clear_types") }),
        z.object({ action: z.literal("enum_types") }),
        z.object({ action: z.literal("load_types"), path: z.string() }),
        z.object({ action: z.literal("parse_types"), path: z.string() }),
        z.object({ action: z.literal("remove_type"), name: z.string() }),
        z.object({ action: z.literal("sizeof_type"), name: z.string() }),
        z.object({ action: z.literal("display_type"), name: z.string() }),
        z.object({ action: z.literal("add_watch"), expression: z.string(), name: z.string().optional() }),
        z.object({ action: z.literal("delete_watch"), id: z.string() }),
        z.object({ action: z.literal("set_watchdog"), id: z.string(), mode: z.string() }),
        z.object({ action: z.literal("set_watch_expression"), id: z.string(), expression: z.string() }),
        z.object({ action: z.literal("set_watch_name"), id: z.string(), name: z.string() }),
        z.object({ action: z.literal("check_watchdog") }),
        z.object({ action: z.literal("dbg_set_value"), name: z.string().describe("Register/variable name"), value: z.string().describe("Value to set") }),
        z.object({ action: z.literal("dbg_set_buffer"), name: z.string().describe("Name"), data: z.string().describe("Hex bytes") }),
        z.object({ action: z.literal("dbg_xref_count"), address: z.string().optional().default("cip") }),
        z.object({ action: z.literal("dbg_xref_type"), address: z.string().optional().default("cip") }),
        z.object({ action: z.literal("dbg_time_wasted") }),
        z.object({ action: z.literal("dbg_watch_list") }),
        z.object({ action: z.literal("dbg_is_run_locked") }),
        z.object({ action: z.literal("dbg_is_valid_expression"), expression: z.string() }),
        z.object({ action: z.literal("dbg_is_valid_read_ptr"), address: z.string() }),
        z.object({ action: z.literal("dbg_bpx_type"), address: z.string() }),
        z.object({ action: z.literal("dbg_is_bp_disabled"), address: z.string() }),
        z.object({ action: z.literal("dbg_encode_type"), address: z.string() }),
        z.object({ action: z.literal("dbg_set_encode_type"), address: z.string(), type: z.string() }),
        z.object({ action: z.literal("dbg_encode_size"), address: z.string() }),
        z.object({ action: z.literal("dbg_arg_type"), address: z.string() }),
        z.object({ action: z.literal("dbg_bookmark_at"), address: z.string() }),
        z.object({ action: z.literal("dbg_comment_at"), address: z.string() }),
        z.object({ action: z.literal("dbg_label_at"), address: z.string() }),
        z.object({ action: z.literal("dbg_loop_type"), address: z.string() }),
        z.object({ action: z.literal("dbg_loop_add"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_loop_del"), address: z.string() }),
        z.object({ action: z.literal("dbg_loop_overlaps"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_function_overlaps"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_argument_overlaps"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_argument_get"), address: z.string() }),
        z.object({ action: z.literal("dbg_clear_auto_bookmark_range"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_clear_auto_comment_range"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_clear_auto_label_range"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_clear_auto_function_range"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_clear_bookmark_range"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_clear_comment_range"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_clear_label_range"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_set_auto_bookmark"), address: z.string() }),
        z.object({ action: z.literal("dbg_set_auto_comment"), address: z.string(), text: z.string() }),
        z.object({ action: z.literal("dbg_set_auto_label"), address: z.string(), text: z.string() }),
        z.object({ action: z.literal("dbg_set_auto_function"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_del_encode_type_range"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_del_encode_type_segment"), start: z.string(), end: z.string() }),
        z.object({ action: z.literal("dbg_exit") }),
        z.object({ action: z.literal("dbg_init") }),
        z.object({ action: z.literal("dbg_settings_updated") }),
        z.object({ action: z.literal("plugin_logprintf"), text: z.string() }),
        z.object({ action: z.literal("plugin_debug_pause") }),
        z.object({ action: z.literal("plugin_debug_skip_exceptions"), skip: z.boolean().optional().default(true) }),
        z.object({ action: z.literal("plugin_wait_until_paused") }),
        z.object({ action: z.literal("plugin_hash"), data: z.string() }),
        z.object({ action: z.literal("bridge_setting_get"), section: z.string(), key: z.string() }),
        z.object({ action: z.literal("bridge_setting_set"), section: z.string(), key: z.string(), value: z.string() }),
        z.object({ action: z.literal("bridge_setting_get_uint"), section: z.string(), key: z.string() }),
        z.object({ action: z.literal("bridge_setting_set_uint"), section: z.string(), key: z.string(), value: z.number() }),
        z.object({ action: z.literal("bridge_setting_flush") }),
        z.object({ action: z.literal("bridge_nt_build") }),
        z.object({ action: z.literal("bridge_working_directory") }),
        z.object({ action: z.literal("bridge_user_directory") }),
        z.object({ action: z.literal("bridge_is_arm64") }),
        z.object({ action: z.literal("plugin_logputs"), text: z.string() }),
        z.object({ action: z.literal("plugin_logprint"), text: z.string() }),
        z.object({ action: z.literal("plugin_load"), name: z.string() }),
        z.object({ action: z.literal("plugin_unload"), name: z.string() }),
        z.object({ action: z.literal("dbg_def_jit"), x64: z.string().optional().default("true") }),
        z.object({ action: z.literal("dbg_set_party"), base: z.string(), party: z.enum(['user', 'system']) }),
        z.object({ action: z.literal("dbg_symbol_status"), module: z.string() }),
        z.object({ action: z.literal("dbg_mem_bp_size"), address: z.string() }),
        z.object({ action: z.literal("dbg_refresh_modules") })
      ])
    },
    async ({ action }) => {
      let data: any;
      switch (action.action) {
        case 'execute':
          data = await httpClient.post('/api/command/exec', { command: action.command });
          break;
        case 'script':
          data = await httpClient.post('/api/command/script', { commands: action.commands });
          break;
        case 'evaluate':
          data = await httpClient.post('/api/command/eval', { expression: action.expression });
          break;
        case 'format':
          data = await httpClient.post('/api/command/format', { format: action.format });
          break;
        case 'set_init_script':
          data = await httpClient.post('/api/command/init_script', { file: action.file });
          break;
        case 'get_init_script':
          data = await httpClient.get('/api/command/init_script');
          break;
        case 'get_hash':
          data = await httpClient.get('/api/command/hash');
          break;
        case 'get_events':
          data = await httpClient.get('/api/command/events');
          break;
        case 'execute_batch':
          data = await httpClient.post('/api/command/execute_batch', { commands: action.commands });
          break;
        case 'last_result':
          data = await httpClient.get('/api/command/last_result');
          break;
        case 'loadlib':
          data = await httpClient.post('/api/misc/loadlib', { dll_path: action.dll_path });
          break;
        case 'graph':
          data = await httpClient.post('/api/misc/graph', { address: action.address });
          break;
        case 'gpa':
          data = await httpClient.post('/api/misc/gpa', { name: action.name, module: action.module });
          break;
        case 'script_exec':
          data = await httpClient.post('/api/command/script_exec', { file: action.file });
          break;
        case 'database_save':
          data = await httpClient.post('/api/database/save', { file: action.file });
          break;
        case 'database_load':
          data = await httpClient.post('/api/database/load', { file: action.file });
          break;
        case 'imageinfo':
          data = await httpClient.get('/api/misc/imageinfo', { module: action.module });
          break;
        case 'symdownload':
          data = await httpClient.post('/api/misc/symdownload', { module: action.module, store: action.store });
          break;
        case 'symunload':
          data = await httpClient.post('/api/misc/symunload', { module: action.module });
          break;
        case 'exinfo':
          data = await httpClient.get('/api/misc/exinfo');
          break;
        case 'exhandlers':
          data = await httpClient.get('/api/misc/exhandlers');
          break;
        case 'virtualmod':
          data = await httpClient.post('/api/misc/virtualmod', { address: action.address, size: action.size });
          break;
        case 'findguid':
          data = await httpClient.post('/api/misc/findguid', { guid: action.guid });
          break;
        case 'modcallfind':
          data = await httpClient.post('/api/misc/modcallfind', { module: action.module });
          break;
        case 'argument_add':
          data = await httpClient.post('/api/misc/argument_add', { address: action.address, name: action.name });
          break;
        case 'argument_del':
          data = await httpClient.post('/api/misc/argument_del', { address: action.address });
          break;
        case 'argument_clear':
          data = await httpClient.post('/api/misc/argument_clear');
          break;
        case 'argument_list':
          data = await httpClient.get('/api/misc/argument_list');
          break;
        case 'comment_clear':
          data = await httpClient.post('/api/misc/comment_clear');
          break;
        case 'label_clear':
          data = await httpClient.post('/api/misc/label_clear');
          break;
        case 'function_clear':
          data = await httpClient.post('/api/misc/function_clear');
          break;
        case 'bookmark_clear':
          data = await httpClient.post('/api/misc/bookmark_clear');
          break;
        case 'set_thread_name':
          data = await httpClient.post('/api/misc/set_thread_name', { tid: action.tid, name: action.name });
          break;
        case 'set_thread_priority':
          data = await httpClient.post('/api/misc/set_thread_priority', { tid: action.tid, priority: action.priority });
          break;
        case 'gui_update_disable':
          data = await httpClient.post('/api/misc/gui_update_disable');
          break;
        case 'gui_update_enable':
          data = await httpClient.post('/api/misc/gui_update_enable');
          break;
        case 'get_jit':
          data = await httpClient.get('/api/misc/get_jit');
          break;
        case 'set_jit':
          data = await httpClient.post('/api/misc/set_jit', { debugger: action.debugger });
          break;
        case 'get_jit_auto':
          data = await httpClient.get('/api/misc/get_jit_auto');
          break;
        case 'set_jit_auto':
          data = await httpClient.post('/api/misc/set_jit_auto', { enabled: action.enabled });
          break;
        case 'plugload':
          data = await httpClient.post('/api/misc/plugload', { path: action.path });
          break;
        case 'plugunload':
          data = await httpClient.post('/api/misc/plugunload', { name: action.name });
          break;
        case 'zzz':
          data = await httpClient.post('/api/misc/zzz', { ms: action.ms });
          break;
        case 'reffind':
          data = await httpClient.post('/api/search/reffind', { value: action.value });
          break;
        case 'refstr':
          data = await httpClient.post('/api/search/refstr');
          break;
        case 'findasm':
          data = await httpClient.post('/api/search/findasm', { instruction: action.instruction });
          break;
        case 'config':
          data = await httpClient.post('/api/misc/config', { action: action.config_action, key: action.key, value: action.value });
          break;
        case 'mnemonichelp':
          data = await httpClient.get('/api/misc/mnemonichelp', { mnemonic: action.mnemonic });
          break;
        case 'script_load':
          data = await httpClient.post('/api/command/script_load', { file: action.file });
          break;
        case 'script_run':
          data = await httpClient.post('/api/command/script_run');
          break;
        case 'comment_list':
          data = await httpClient.get('/api/comments/list');
          break;
        case 'label_list':
          data = await httpClient.get('/api/labels/list');
          break;
        case 'bpgoto':
          data = await httpClient.post('/api/misc/bpgoto', { address: action.address, target: action.target });
          break;
        case 'chd':
          data = await httpClient.post('/api/misc/chd', { path: action.path });
          break;
        case 'memmapdump':
          data = await httpClient.post('/api/misc/memmapdump', { address: action.address });
          break;
        case 'sdump':
          data = await httpClient.post('/api/misc/sdump', { address: action.address });
          break;
        case 'set_freezestack':
          data = await httpClient.post('/api/misc/set_freezestack', { freeze: action.freeze });
          break;
        case 'database_clear':
          data = await httpClient.post('/api/database/clear');
          break;
        case 'label_delete':
          data = await httpClient.post('/api/labels/delete', { address: action.address });
          break;
        case 'variable_new':
          data = await httpClient.post('/api/variables/new', { name: action.name, value: action.value });
          break;
        case 'variable_delete':
          data = await httpClient.post('/api/variables/delete', { name: action.name });
          break;
        case 'variable_list':
          data = await httpClient.get('/api/variables/list');
          break;
        case 'traceexecute':
          data = await httpClient.post('/api/misc/traceexecute', { address: action.address });
          break;
        case 'script_cmd':
          data = await httpClient.post('/api/command/script_cmd', { command: action.command });
          break;
        case 'script_dll':
          data = await httpClient.post('/api/command/script_dll', { path: action.path });
          break;
        case 'reffind_range':
          data = await httpClient.post('/api/search/reffind_range', { start: action.start, end: action.end });
          break;
        case 'set_max_results':
          data = await httpClient.post('/api/search/set_max_results', { max_results: action.max_results });
          break;
        case 'refinit':
          data = await httpClient.post('/api/misc/refinit');
          break;
        case 'refadd':
          data = await httpClient.post('/api/misc/refadd', { address: action.address, text: action.text });
          break;
        case 'refget':
          data = await httpClient.get('/api/misc/refget', { address: action.address });
          break;
        case 'hide_debugger':
          data = await httpClient.post('/api/misc/hide_debugger');
          break;
        case 'clear_log':
          data = await httpClient.post('/api/misc/clear_log');
          break;
        case 'disable_log':
          data = await httpClient.post('/api/misc/disable_log');
          break;
        case 'enable_log':
          data = await httpClient.post('/api/misc/enable_log');
          break;
        case 'get_reloc_size':
          data = await httpClient.get('/api/misc/get_reloc_size', { module: action.module });
          break;
        case 'set_bp_options':
          data = await httpClient.post('/api/misc/set_bp_options', { type: action.type });
          break;
        case 'enable_privilege':
          data = await httpClient.post('/api/misc/enable_privilege', { privilege: action.privilege });
          break;
        case 'disable_privilege':
          data = await httpClient.post('/api/misc/disable_privilege', { privilege: action.privilege });
          break;
        case 'get_privilege_state':
          data = await httpClient.get('/api/misc/get_privilege_state', { privilege: action.privilege });
          break;
        case 'fold_disassembly':
          data = await httpClient.post('/api/misc/fold_disassembly', { start: action.start, end: action.end });
          break;
        case 'set_memory_range_bp':
          data = await httpClient.post('/api/breakpoints/set_memory_range', { address: action.address, size: action.size });
          break;
        case 'librarian_set_bp':
          data = await httpClient.post('/api/breakpoints/librarian_set', { name: action.name });
          break;
        case 'librarian_remove_bp':
          data = await httpClient.post('/api/breakpoints/librarian_remove', { name: action.name });
          break;
        case 'librarian_enable_bp':
          data = await httpClient.post('/api/breakpoints/librarian_enable', { name: action.name });
          break;
        case 'librarian_disable_bp':
          data = await httpClient.post('/api/breakpoints/librarian_disable', { name: action.name });
          break;
        case 'get_bp_hitcount':
          data = await httpClient.get('/api/breakpoints/hitcount', { type: action.type, address: action.address });
          break;
        case 'reset_bp_hitcount':
          data = await httpClient.post('/api/breakpoints/reset_hitcount_by_type', { type: action.type, address: action.address });
          break;
        case 'add_favourite_command':
          data = await httpClient.post('/api/misc/add_favourite_command', { command: action.command });
          break;
        case 'add_favourite_tool':
          data = await httpClient.post('/api/misc/add_favourite_tool', { path: action.path, description: action.description });
          break;
        case 'add_favourite_shortcut':
          data = await httpClient.post('/api/misc/add_favourite_shortcut', { shortcut: action.shortcut, description: action.description });
          break;
        case 'start_scylla':
          data = await httpClient.post('/api/misc/start_scylla');
          break;
        case 'set_data_type':
          data = await httpClient.post('/api/types/set_data', { type: action.type, address: action.address });
          break;
        case 'add_struct':
          data = await httpClient.post('/api/types/add_struct', { name: action.name });
          break;
        case 'add_union':
          data = await httpClient.post('/api/types/add_union', { name: action.name });
          break;
        case 'add_type_alias':
          data = await httpClient.post('/api/types/add_type', { name: action.name, type: action.type });
          break;
        case 'add_function_type':
          data = await httpClient.post('/api/types/add_function', { name: action.name, return_type: action.return_type, args: action.args });
          break;
        case 'add_arg':
          data = await httpClient.post('/api/types/add_arg', { name: action.name, type: action.type });
          break;
        case 'add_member':
          data = await httpClient.post('/api/types/add_member', { name: action.name, type: action.type });
          break;
        case 'append_arg':
          data = await httpClient.post('/api/types/append_arg', { name: action.name, type: action.type });
          break;
        case 'append_member':
          data = await httpClient.post('/api/types/append_member', { name: action.name, type: action.type });
          break;
        case 'clear_types':
          data = await httpClient.post('/api/types/clear');
          break;
        case 'enum_types':
          data = await httpClient.get('/api/types/enum');
          break;
        case 'load_types':
          data = await httpClient.post('/api/types/load', { path: action.path });
          break;
        case 'parse_types':
          data = await httpClient.post('/api/types/parse', { path: action.path });
          break;
        case 'remove_type':
          data = await httpClient.post('/api/types/remove', { name: action.name });
          break;
        case 'sizeof_type':
          data = await httpClient.get('/api/types/sizeof', { name: action.name });
          break;
        case 'display_type':
          data = await httpClient.get('/api/types/display', { name: action.name });
          break;
        case 'add_watch':
          data = await httpClient.post('/api/watch/add', { expression: action.expression, name: action.name });
          break;
        case 'delete_watch':
          data = await httpClient.post('/api/watch/delete', { id: action.id });
          break;
        case 'set_watchdog':
          data = await httpClient.post('/api/watch/set_watchdog', { id: action.id, mode: action.mode });
          break;
        case 'set_watch_expression':
          data = await httpClient.post('/api/watch/set_expression', { id: action.id, expression: action.expression });
          break;
        case 'set_watch_name':
          data = await httpClient.post('/api/watch/set_name', { id: action.id, name: action.name });
          break;
        case 'check_watchdog':
          data = await httpClient.get('/api/watch/check_watchdog');
          break;
        case 'dbg_set_value':
          data = await httpClient.post('/api/dbg/set_value', { name: action.name, value: action.value });
          break;
        case 'dbg_set_buffer':
          data = await httpClient.post('/api/dbg/set_buffer', { name: action.name, data: action.data });
          break;
        case 'dbg_xref_count':
          data = await httpClient.get('/api/dbg/xref_count', { address: action.address });
          break;
        case 'dbg_xref_type':
          data = await httpClient.get('/api/dbg/xref_type', { address: action.address });
          break;
        case 'dbg_time_wasted':
          data = await httpClient.get('/api/dbg/time_wasted');
          break;
        case 'dbg_watch_list':
          data = await httpClient.get('/api/dbg/watch_list');
          break;
        case 'dbg_is_run_locked':
          data = await httpClient.get('/api/dbg/is_run_locked');
          break;
        case 'dbg_is_valid_expression':
          data = await httpClient.get('/api/dbg/is_valid_expression', { expression: action.expression });
          break;
        case 'dbg_is_valid_read_ptr':
          data = await httpClient.get('/api/dbg/is_valid_read_ptr', { address: action.address });
          break;
        case 'dbg_bpx_type':
          data = await httpClient.get('/api/dbg/bpx_type', { address: action.address });
          break;
        case 'dbg_is_bp_disabled':
          data = await httpClient.get('/api/dbg/is_bp_disabled', { address: action.address });
          break;
        case 'dbg_encode_type':
          data = await httpClient.get('/api/dbg/encode_type', { address: action.address });
          break;
        case 'dbg_set_encode_type':
          data = await httpClient.post('/api/dbg/set_encode_type', { address: action.address, type: action.type });
          break;
        case 'dbg_encode_size':
          data = await httpClient.get('/api/dbg/encode_size', { address: action.address });
          break;
        case 'dbg_arg_type':
          data = await httpClient.get('/api/dbg/arg_type', { address: action.address });
          break;
        case 'dbg_bookmark_at':
          data = await httpClient.get('/api/dbg/bookmark_at', { address: action.address });
          break;
        case 'dbg_comment_at':
          data = await httpClient.get('/api/dbg/comment_at', { address: action.address });
          break;
        case 'dbg_label_at':
          data = await httpClient.get('/api/dbg/label_at', { address: action.address });
          break;
        case 'dbg_loop_type':
          data = await httpClient.get('/api/dbg/loop_type', { address: action.address });
          break;
        case 'dbg_loop_add':
          data = await httpClient.post('/api/dbg/loop_add', { start: action.start, end: action.end });
          break;
        case 'dbg_loop_del':
          data = await httpClient.post('/api/dbg/loop_del', { address: action.address });
          break;
        case 'dbg_loop_overlaps':
          data = await httpClient.get('/api/dbg/loop_overlaps', { start: action.start, end: action.end });
          break;
        case 'dbg_function_overlaps':
          data = await httpClient.get('/api/dbg/function_overlaps', { start: action.start, end: action.end });
          break;
        case 'dbg_argument_overlaps':
          data = await httpClient.get('/api/dbg/argument_overlaps', { start: action.start, end: action.end });
          break;
        case 'dbg_argument_get':
          data = await httpClient.get('/api/dbg/argument_get', { address: action.address });
          break;
        case 'dbg_clear_auto_bookmark_range':
          data = await httpClient.post('/api/dbg/clear_auto_bookmark_range', { start: action.start, end: action.end });
          break;
        case 'dbg_clear_auto_comment_range':
          data = await httpClient.post('/api/dbg/clear_auto_comment_range', { start: action.start, end: action.end });
          break;
        case 'dbg_clear_auto_label_range':
          data = await httpClient.post('/api/dbg/clear_auto_label_range', { start: action.start, end: action.end });
          break;
        case 'dbg_clear_auto_function_range':
          data = await httpClient.post('/api/dbg/clear_auto_function_range', { start: action.start, end: action.end });
          break;
        case 'dbg_clear_bookmark_range':
          data = await httpClient.post('/api/dbg/clear_bookmark_range', { start: action.start, end: action.end });
          break;
        case 'dbg_clear_comment_range':
          data = await httpClient.post('/api/dbg/clear_comment_range', { start: action.start, end: action.end });
          break;
        case 'dbg_clear_label_range':
          data = await httpClient.post('/api/dbg/clear_label_range', { start: action.start, end: action.end });
          break;
        case 'dbg_set_auto_bookmark':
          data = await httpClient.post('/api/dbg/set_auto_bookmark', { address: action.address });
          break;
        case 'dbg_set_auto_comment':
          data = await httpClient.post('/api/dbg/set_auto_comment', { address: action.address, text: action.text });
          break;
        case 'dbg_set_auto_label':
          data = await httpClient.post('/api/dbg/set_auto_label', { address: action.address, text: action.text });
          break;
        case 'dbg_set_auto_function':
          data = await httpClient.post('/api/dbg/set_auto_function', { start: action.start, end: action.end });
          break;
        case 'dbg_del_encode_type_range':
          data = await httpClient.post('/api/dbg/del_encode_type_range', { start: action.start, end: action.end });
          break;
        case 'dbg_del_encode_type_segment':
          data = await httpClient.post('/api/dbg/del_encode_type_segment', { start: action.start, end: action.end });
          break;
        case 'dbg_exit':
          data = await httpClient.post('/api/dbg/exit');
          break;
        case 'dbg_init':
          data = await httpClient.post('/api/dbg/init');
          break;
        case 'dbg_settings_updated':
          data = await httpClient.post('/api/dbg/settings_updated');
          break;
        case 'plugin_logprintf':
          data = await httpClient.post('/api/plugin/logprintf', { text: action.text });
          break;
        case 'plugin_debug_pause':
          data = await httpClient.post('/api/plugin/debug_pause');
          break;
        case 'plugin_debug_skip_exceptions':
          data = await httpClient.post('/api/plugin/debug_skip_exceptions', { skip: action.skip });
          break;
        case 'plugin_wait_until_paused':
          data = await httpClient.post('/api/plugin/wait_until_paused');
          break;
        case 'plugin_hash':
          data = await httpClient.post('/api/plugin/hash', { data: action.data });
          break;
        case 'bridge_setting_get':
          data = await httpClient.get('/api/bridge/setting', { section: action.section, key: action.key });
          break;
        case 'bridge_setting_set':
          data = await httpClient.post('/api/bridge/setting', { section: action.section, key: action.key, value: action.value });
          break;
        case 'bridge_setting_get_uint':
          data = await httpClient.get('/api/bridge/setting_uint', { section: action.section, key: action.key });
          break;
        case 'bridge_setting_set_uint':
          data = await httpClient.post('/api/bridge/setting_uint', { section: action.section, key: action.key, value: action.value });
          break;
        case 'bridge_setting_flush':
          data = await httpClient.post('/api/bridge/setting_flush');
          break;
        case 'bridge_nt_build':
          data = await httpClient.get('/api/bridge/nt_build');
          break;
        case 'bridge_working_directory':
          data = await httpClient.get('/api/bridge/working_directory');
          break;
        case 'bridge_user_directory':
          data = await httpClient.get('/api/bridge/user_directory');
          break;
        case 'bridge_is_arm64':
          data = await httpClient.get('/api/bridge/is_arm64');
          break;
        case 'plugin_logputs':
          data = await httpClient.post('/api/plugin/logputs', { text: action.text });
          break;
        case 'plugin_logprint':
          data = await httpClient.post('/api/plugin/logprint', { text: action.text });
          break;
        case 'plugin_load':
          data = await httpClient.post('/api/plugin/load', { name: action.name });
          break;
        case 'plugin_unload':
          data = await httpClient.post('/api/plugin/unload', { name: action.name });
          break;
        case 'dbg_def_jit':
          data = await httpClient.get('/api/dbg/def_jit', { x64: action.x64 });
          break;
        case 'dbg_set_party':
          data = await httpClient.post('/api/dbg/set_party', { base: action.base, party: action.party });
          break;
        case 'dbg_symbol_status':
          data = await httpClient.get('/api/dbg/symbol_status', { module: action.module });
          break;
        case 'dbg_mem_bp_size':
          data = await httpClient.get('/api/dbg/mem_bp_size', { address: action.address });
          break;
        case 'dbg_refresh_modules':
          data = await httpClient.post('/api/dbg/refresh_modules');
          break;
      }
      return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
    }
  );
}

export function registerPatternSearchTool(server: McpServer) {
  server.tool('x64dbg_find_pattern', 'Search for assembly instruction patterns across the binary. Supports indirect calls, stack accesses, API calls, jump tables, and custom patterns.', {
    type: z.enum(['call_indirect', 'stack_access', 'api_call', 'jmp_table', 'custom']).describe('Type of assembly pattern to search for'),
    pattern: z.string().optional().describe('Register or offset pattern (e.g., "e" for [eax*4], "arg_0" for [ebp+arg_0])'),
    api: z.string().optional().describe('API function name for api_call pattern type')
  }, async ({ type, pattern, api }) => {
    const data = await httpClient.post('/api/search/pattern_asm', { type, pattern, api });
    return { content: [{ type: 'text', text: JSON.stringify(data, null, 2) }] };
  });
}