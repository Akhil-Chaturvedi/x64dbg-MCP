#include "http/c_http_router.h"
#include "bridge/c_bridge_executor.h"
#include "util/format_utils.h"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include "_dbgfunctions.h"
#include "_plugins.h"

namespace handlers {

void register_extras_routes(c_http_router& router) {
    // POST /api/memory/fill - Fill memory with a byte value (Fill/memset)
    router.post("/api/memory/fill", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("value") || !body.contains("size")) {
            return s_http_response::bad_request("Missing 'address', 'value', and/or 'size' fields");
        }

        auto address_str = body["address"].get<std::string>();
        auto value_str = body["value"].get<std::string>();
        auto size_str = body["size"].get<std::string>();

        auto cmd = "Fill " + address_str + ", " + size_str + ", " + value_str;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({
            {"success", success},
            {"command", cmd}
        });
    });

    // POST /api/memory/memcpy - Copy memory without patches
    router.post("/api/memory/memcpy", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("dest") || !body.contains("src") || !body.contains("size")) {
            return s_http_response::bad_request("Missing 'dest', 'src', and/or 'size' fields");
        }

        auto dest = body["dest"].get<std::string>();
        auto src = body["src"].get<std::string>();
        auto size = body["size"].get<std::string>();

        auto cmd = "memcpy " + dest + ", " + src + ", " + size;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // GET /api/memory/page_rights?address= - Get page rights
    router.get("/api/memory/page_rights", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto address_str = req.get_query("address");
        if (address_str.empty()) {
            return s_http_response::bad_request("Missing 'address' query parameter");
        }

        auto address = bridge.eval_expression(address_str);
        char rights[64] = {};
        auto found = DbgFunctions()->GetPageRights(address, rights);

        return s_http_response::ok({
            {"address", format_utils::format_address(address)},
            {"rights",  found ? std::string(rights) : "unknown"},
            {"found",   found}
        });
    });

    // POST /api/memory/set_page_rights - Set page rights
    router.post("/api/memory/set_page_rights", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("rights")) {
            return s_http_response::bad_request("Missing 'address' and/or 'rights' fields");
        }

        auto address_str = body["address"].get<std::string>();
        auto rights = body["rights"].get<std::string>();

        auto cmd = "setpagerights " + address_str + ", " + rights;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/dump/minidump - Create minidump with full memory
    router.post("/api/dump/minidump", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto file = body.value("file", std::string());

        std::string cmd = "minidump";
        if (!file.empty()) {
            cmd += " \"" + file + "\"";
        }
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({
            {"success", success},
            {"file", file.empty() ? "(default path)" : file}
        });
    });

    // POST /api/misc/loadlib - Load DLL into debuggee
    router.post("/api/misc/loadlib", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("dll_path")) {
            return s_http_response::bad_request("Missing 'dll_path' field");
        }

        auto dll_path = body["dll_path"].get<std::string>();
        auto cmd = "loadlib \"" + dll_path + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/gpa - Get address of export in DLL
    router.post("/api/misc/gpa", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("module")) {
            return s_http_response::bad_request("Missing 'name' and/or 'module' fields");
        }

        auto name = body["name"].get<std::string>();
        auto module = body["module"].get<std::string>();

        auto cmd = "gpa \"" + name + "\", \"" + module + "\"";
        auto success = bridge.exec_command(cmd);
        auto result = bridge.eval_expression("$result");

        return s_http_response::ok({
            {"success", success},
            {"name",    name},
            {"module",  module},
            {"address", format_utils::format_address(result)}
        });
    });

    // POST /api/threads/create - Create a new thread in debuggee
    router.post("/api/threads/create", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("entry")) {
            return s_http_response::bad_request("Missing 'entry' field");
        }

        auto entry = body["entry"].get<std::string>();
        auto cmd = "createthread " + entry;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/graph - Graph control flow at address
    router.post("/api/misc/graph", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto address_str = body.value("address", std::string("cip"));

        auto cmd = "graph " + address_str;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"address", address_str}});
    });

    // POST /api/analysis/run_analysis - Run full analysis on module
    router.post("/api/analysis/run_analysis", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto module = body.value("module", std::string());
        auto type = body.value("type", std::string("full"));

        std::string cmd;
        if (type == "control_flow") {
            cmd = "cfanalyze";
        } else if (type == "nukem") {
            cmd = "analyse_nukem";
        } else {
            cmd = "analyse";
        }
        if (!module.empty()) {
            cmd += " " + module;
        }
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"type", type}, {"command", cmd}});
    });

    // POST /api/command/script_exec - Load and execute a script file
    router.post("/api/command/script_exec", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("file")) {
            return s_http_response::bad_request("Missing 'file' field");
        }

        auto file = body["file"].get<std::string>();
        auto cmd = "scriptexec \"" + file + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"file", file}});
    });

    // POST /api/database/save - Save database to disk
    router.post("/api/database/save", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto file = body.value("file", std::string());

        std::string cmd = "savedb";
        if (!file.empty()) {
            cmd += " \"" + file + "\"";
        }
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}});
    });

    // POST /api/database/load - Load database from disk
    router.post("/api/database/load", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("file")) {
            return s_http_response::bad_request("Missing 'file' field");
        }

        auto file = body["file"].get<std::string>();
        auto cmd = "loaddb \"" + file + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"file", file}});
    });

    // GET /api/misc/imageinfo?module= - PE image information
    router.get("/api/misc/imageinfo", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto module = req.get_query("module", "0");
        auto cmd = "imageinfo " + module;
        auto success = bridge.exec_command(cmd);
        auto result = bridge.eval_expression("$result");

        return s_http_response::ok({
            {"success", success},
            {"module", module},
            {"result", result}
        });
    });

    // POST /api/misc/symdownload - Download symbols from symbol store
    router.post("/api/misc/symdownload", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto module = body.value("module", std::string());
        auto store = body.value("store", std::string());

        std::string cmd = "symdownload";
        if (!module.empty()) cmd += " " + module;
        if (!store.empty()) cmd += ", " + store;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/symunload - Unload symbols
    router.post("/api/misc/symunload", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto module = body.value("module", std::string());

        std::string cmd = "symunload";
        if (!module.empty()) cmd += " " + module;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // GET /api/misc/exinfo - Get last exception debug info
    router.get("/api/misc/exinfo", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto success = bridge.exec_command("exinfo");
        return s_http_response::ok({{"success", success}});
    });

    // GET /api/misc/exhandlers - List exception handlers (SEH/VEH/VCH)
    router.get("/api/misc/exhandlers", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto success = bridge.exec_command("exhandlers");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/virtualmod - Treat memory range as virtual module
    router.post("/api/misc/virtualmod", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("size")) {
            return s_http_response::bad_request("Missing 'address' and/or 'size' fields");
        }

        auto address = body["address"].get<std::string>();
        auto size = body["size"].get<std::string>();
        auto cmd = "virtualmod " + address + ", " + size;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/findguid - Find GUID references
    router.post("/api/misc/findguid", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("guid")) {
            return s_http_response::bad_request("Missing 'guid' field");
        }

        auto guid = body["guid"].get<std::string>();
        auto cmd = "findguid " + guid;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"guid", guid}});
    });

    // POST /api/misc/modcallfind - Find inter-modular calls
    router.post("/api/misc/modcallfind", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto module = body.value("module", std::string());

        std::string cmd = "modcallfind";
        if (!module.empty()) cmd += " " + module;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/argument_add - Add function argument
    router.post("/api/misc/argument_add", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("name")) {
            return s_http_response::bad_request("Missing 'address' and/or 'name' fields");
        }

        auto address = body["address"].get<std::string>();
        auto name = body["name"].get<std::string>();
        auto cmd = "argumentadd " + address + ", " + name;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/argument_del - Delete function argument
    router.post("/api/misc/argument_del", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address")) {
            return s_http_response::bad_request("Missing 'address' field");
        }

        auto address = body["address"].get<std::string>();
        auto cmd = "argumentdel " + address;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/argument_clear - Clear all arguments
    router.post("/api/misc/argument_clear", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto success = bridge.exec_command("argumentclear");
        return s_http_response::ok({{"success", success}});
    });

    // GET /api/misc/argument_list - List arguments
    router.get("/api/misc/argument_list", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto success = bridge.exec_command("argumentlist");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/comment_clear - Clear all comments
    router.post("/api/misc/comment_clear", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto success = bridge.exec_command("commentclear");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/label_clear - Clear all labels
    router.post("/api/misc/label_clear", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto success = bridge.exec_command("labelclear");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/function_clear - Clear all functions
    router.post("/api/misc/function_clear", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto success = bridge.exec_command("functionclear");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/bookmark_clear - Clear all bookmarks
    router.post("/api/misc/bookmark_clear", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto success = bridge.exec_command("bookmarkclear");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/set_thread_name - Set thread name
    router.post("/api/misc/set_thread_name", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("tid") || !body.contains("name")) {
            return s_http_response::bad_request("Missing 'tid' and/or 'name' fields");
        }

        auto tid = body["tid"].get<std::string>();
        auto name = body["name"].get<std::string>();
        auto cmd = "setthreadname " + tid + ", " + name;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/set_thread_priority - Set thread priority
    router.post("/api/misc/set_thread_priority", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("tid") || !body.contains("priority")) {
            return s_http_response::bad_request("Missing 'tid' and/or 'priority' fields");
        }

        auto tid = body["tid"].get<std::string>();
        auto priority = body["priority"].get<std::string>();
        auto cmd = "setthreadpriority " + tid + ", " + priority;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/gui_update_disable - Disable GUI updates
    router.post("/api/misc/gui_update_disable", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("guiupdatedisable");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/gui_update_enable - Enable GUI updates
    router.post("/api/misc/gui_update_enable", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("guiupdateenable");
        return s_http_response::ok({{"success", success}});
    });

    // GET /api/misc/get_jit - Get JIT debugger settings
    router.get("/api/misc/get_jit", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("getjit");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/set_jit - Set JIT debugger
    router.post("/api/misc/set_jit", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("debugger")) {
            return s_http_response::bad_request("Missing 'debugger' field");
        }

        auto debugger = body["debugger"].get<std::string>();
        auto cmd = "setjit \"" + debugger + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // GET /api/misc/get_jit_auto - Get JIT auto flag
    router.get("/api/misc/get_jit_auto", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("getjitauto");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/set_jit_auto - Set JIT auto flag
    router.post("/api/misc/set_jit_auto", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("enabled")) {
            return s_http_response::bad_request("Missing 'enabled' field");
        }

        auto enabled = body["enabled"].get<std::string>();
        auto cmd = "setjitauto " + enabled;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/plugload - Load a plugin
    router.post("/api/misc/plugload", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("path")) {
            return s_http_response::bad_request("Missing 'path' field");
        }

        auto path = body["path"].get<std::string>();
        auto cmd = "plugload \"" + path + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/plugunload - Unload a plugin
    router.post("/api/misc/plugunload", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) {
            return s_http_response::bad_request("Missing 'name' field");
        }

        auto name = body["name"].get<std::string>();
        auto cmd = "plugunload \"" + name + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/zzz - Sleep/halt execution
    router.post("/api/misc/zzz", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto ms = body.value("ms", std::string("1000"));

        auto cmd = "zzz " + ms;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"ms", ms}});
    });

    // POST /api/debug/erun - Exception-aware run (passes exceptions to debuggee)
    router.post("/api/debug/erun", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("erun");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/debug/estep_into - Exception-aware step into
    router.post("/api/debug/estep_into", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("esti");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/debug/estep_over - Exception-aware step over
    router.post("/api/debug/estep_over", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("esto");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/debug/estep_out - Exception-aware step out
    router.post("/api/debug/estep_out", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("ertr");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/debug/serun - Exception-swallowing run
    router.post("/api/debug/serun", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("serun");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/debug/sestep_into - Exception-swallowing step into
    router.post("/api/debug/sestep_into", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("sesti");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/debug/sestep_over - Exception-swallowing step over
    router.post("/api/debug/sestep_over", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("sesto");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/threads/kill - Kill a thread
    router.post("/api/threads/kill", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("id")) {
            return s_http_response::bad_request("Missing 'id' field");
        }

        auto tid = body["id"].get<std::string>();
        auto cmd = "killthread " + tid;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"tid", tid}});
    });

    // POST /api/threads/resume_all - Resume all threads
    router.post("/api/threads/resume_all", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto success = bridge.exec_command("resumeallthreads");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/threads/suspend_all - Suspend all threads
    router.post("/api/threads/suspend_all", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto success = bridge.exec_command("suspendallthreads");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/search/reffind - Find references to a value
    router.post("/api/search/reffind", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("value")) {
            return s_http_response::bad_request("Missing 'value' field");
        }

        auto value = body["value"].get<std::string>();
        auto cmd = "reffind " + value;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"value", value}});
    });

    // POST /api/search/refstr - Find referenced strings
    router.post("/api/search/refstr", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto success = bridge.exec_command("refstr");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/search/findasm - Find assembled instruction
    router.post("/api/search/findasm", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("instruction")) {
            return s_http_response::bad_request("Missing 'instruction' field");
        }

        auto instruction = body["instruction"].get<std::string>();
        auto cmd = "findasm \"" + instruction + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"instruction", instruction}});
    });

    // POST /api/misc/config - Get or set x64dbg configuration
    router.post("/api/misc/config", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return s_http_response::bad_request("Invalid JSON body");
        }

        auto action = body.value("action", std::string("get"));
        std::string cmd;
        if (action == "get") {
            auto key = body.value("key", std::string());
            cmd = "config get";
            if (!key.empty()) cmd += " " + key;
        } else if (action == "set") {
            if (!body.contains("key") || !body.contains("value")) {
                return s_http_response::bad_request("Missing 'key' and/or 'value' fields for set action");
            }
            auto key = body["key"].get<std::string>();
            auto value = body["value"].get<std::string>();
            cmd = "config set " + key + ", " + value;
        } else {
            return s_http_response::bad_request("Action must be 'get' or 'set'");
        }

        auto success = bridge.exec_command(cmd);
        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // GET /api/misc/mnemonichelp - Get detailed mnemonic help
    router.get("/api/misc/mnemonichelp", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto mnemonic = req.get_query("mnemonic");
        if (mnemonic.empty()) {
            return s_http_response::bad_request("Missing 'mnemonic' query parameter");
        }

        auto cmd = "mnemonichelp " + mnemonic;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"mnemonic", mnemonic}});
    });

    // POST /api/command/script_load - Load a script file
    router.post("/api/command/script_load", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("file")) {
            return s_http_response::bad_request("Missing 'file' field");
        }

        auto file = body["file"].get<std::string>();
        auto cmd = "scriptload \"" + file + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"file", file}});
    });

    // POST /api/command/script_run - Run the currently loaded script
    router.post("/api/command/script_run", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("scriptrun");
        return s_http_response::ok({{"success", success}});
    });

    // GET /api/comments/list - List all comments
    router.get("/api/comments/list", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto success = bridge.exec_command("commentlist");
        return s_http_response::ok({{"success", success}});
    });

    // GET /api/labels/list - List all labels
    router.get("/api/labels/list", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto success = bridge.exec_command("labellist");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/analysis/compound - Run multiple analysis passes in one call
    router.post("/api/analysis/compound", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return s_http_response::bad_request("Invalid JSON body");
        }

        auto module = body.value("module", std::string("main"));
        auto passes = body.value("passes", nlohmann::json::array());
        if (passes.empty()) {
            passes = {"functions", "imports", "exports", "sections", "strings", "xrefs"};
        }

        auto result = nlohmann::json::object();
        result["module"] = module;

        for (const auto& pass : passes) {
            auto pass_name = pass.get<std::string>();

            if (pass_name == "functions") {
                auto success = bridge.exec_command("analyse " + module);
                result["analysis"] = {{"success", success}, {"type", "full_analysis"}};
            } else if (pass_name == "imports") {
                auto success = bridge.exec_command("modcallfind " + module);
                result["inter_modular_calls"] = {{"success", success}};
            } else if (pass_name == "exports") {
                auto base = bridge.eval_expression("mod.base(" + module + ")");
                result["module_base"] = format_utils::format_address(base);
            } else if (pass_name == "sections") {
                auto success = bridge.exec_command("imageinfo " + module);
                result["image_info"] = {{"success", success}};
            } else if (pass_name == "strings") {
                auto success = bridge.exec_command("refstr");
                result["referenced_strings"] = {{"success", success}};
            } else if (pass_name == "xrefs") {
                auto cip = bridge.eval_expression("cip");
                auto success = bridge.exec_command("reffind " + format_utils::format_address(cip));
                result["xrefs_at_cip"] = {{"success", success}, {"address", format_utils::format_address(cip)}};
            }
        }

        return s_http_response::ok(result);
    });

    // POST /api/events/subscribe - Subscribe to debugger events
    router.post("/api/events/subscribe", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return s_http_response::bad_request("Invalid JSON body");
        }

        auto event_type = body.value("event", std::string());
        auto condition = body.value("condition", std::string());

        if (event_type.empty()) {
            return s_http_response::bad_request("Missing 'event' field");
        }

        std::string cmd;
        if (event_type == "breakpoint") {
            auto address = body.value("address", std::string("cip"));
            cmd = "SetBreakpointCondition " + address;
            if (!condition.empty()) {
                cmd += ", " + condition;
            }
        } else if (event_type == "exception") {
            auto code = body.value("code", std::string());
            if (code.empty()) {
                return s_http_response::bad_request("Missing 'code' for exception event");
            }
            cmd = "SetExceptionBPX " + code + ", first";
        } else if (event_type == "module_load") {
            auto name = body.value("name", std::string());
            if (name.empty()) {
                return s_http_response::bad_request("Missing 'name' for module_load event");
            }
            cmd = "SetLibrarianBreakpoint " + name;

            // Check if the module is already loaded. If so, the librarian breakpoint
            // will only fire if the module is unloaded and reloaded.
            auto mod_base = DbgFunctions()->ModBaseFromName(name.c_str());
            auto already_loaded = (mod_base != 0);

            auto success = bridge.exec_command(cmd);

            nlohmann::json response = {
                {"success", success},
                {"event", event_type},
                {"name", name},
                {"command", cmd}
            };

            if (already_loaded) {
                response["warning"] = "Module '" + name + "' is already loaded (base="
                    + format_utils::format_address(mod_base)
                    + "). The librarian breakpoint will only fire if the module is unloaded and reloaded.";
            }

            return s_http_response::ok(response);
        } else {
            return s_http_response::bad_request("Unknown event type: " + event_type + ". Supported: breakpoint, exception, module_load");
        }

        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({
            {"success", success},
            {"event", event_type},
            {"command", cmd}
        });
    });

    // POST /api/events/unsubscribe - Remove event subscription
    router.post("/api/events/unsubscribe", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return s_http_response::bad_request("Invalid JSON body");
        }

        auto event_type = body.value("event", std::string());
        auto address = body.value("address", std::string());

        std::string cmd;
        if (event_type == "breakpoint") {
            if (address.empty()) {
                return s_http_response::bad_request("Missing 'address' for breakpoint unsubscribe");
            }
            cmd = "DeleteBPX " + address;
        } else if (event_type == "exception") {
            auto code = body.value("code", std::string());
            if (code.empty()) {
                return s_http_response::bad_request("Missing 'code' for exception unsubscribe");
            }
            cmd = "DeleteExceptionBPX " + code;
        } else if (event_type == "module_load") {
            auto name = body.value("name", std::string());
            if (name.empty()) {
                return s_http_response::bad_request("Missing 'name' for module_load unsubscribe");
            }
            cmd = "DeleteLibrarianBreakpoint " + name;
        } else {
            return s_http_response::bad_request("Unknown event type: " + event_type);
        }

        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({
            {"success", success},
            {"event", event_type},
            {"command", cmd}
        });
    });

    // POST /api/context/snapshot_capture - Capture a named state snapshot
    router.post("/api/context/snapshot_capture", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto label = body.value("label", std::string("snapshot_" + std::to_string(time(nullptr))));

        // Capture registers
        auto reg_dump = bridge.get_register_dump();
        auto registers = nlohmann::json::object();
        if (reg_dump.has_value()) {
            auto& rd = reg_dump.value();
            registers["registers"] = {
                {"cax", format_utils::format_address(rd.regcontext.cax)},
                {"cbx", format_utils::format_address(rd.regcontext.cbx)},
                {"ccx", format_utils::format_address(rd.regcontext.ccx)},
                {"cdx", format_utils::format_address(rd.regcontext.cdx)},
                {"csp", format_utils::format_address(rd.regcontext.csp)},
                {"cbp", format_utils::format_address(rd.regcontext.cbp)},
                {"cip", format_utils::format_address(rd.regcontext.cip)}
            };
        }

        // Capture memory map summary
        auto mem_map = bridge.get_memory_map();
        auto regions = nlohmann::json::array();
        if (mem_map.has_value()) {
            for (const auto& region : mem_map.value()) {
                regions.push_back({
                    {"base", region["base"]},
                    {"size", region["size"]},
                    {"type", region["type"]}
                });
            }
        }

        // Capture breakpoints
        auto bps = bridge.get_breakpoint_list(bp_normal);
        auto breakpoints = nlohmann::json::array();
        if (bps.has_value()) {
            for (const auto& bp : bps.value()) {
                breakpoints.push_back(bp);
            }
        }

        return s_http_response::ok({
            {"label", label},
            {"timestamp", time(nullptr)},
            {"registers", registers},
            {"memory_regions", regions},
            {"breakpoints", breakpoints}
        });
    });

    // POST /api/context/snapshot_compare - Compare two named snapshots
    router.post("/api/context/snapshot_compare", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return s_http_response::bad_request("Invalid JSON body");
        }

        auto snapshot1 = body.value("snapshot1", nlohmann::json::object());
        auto snapshot2 = body.value("snapshot2", nlohmann::json::object());

        if (snapshot1.empty() || snapshot2.empty()) {
            return s_http_response::bad_request("Both 'snapshot1' and 'snapshot2' are required");
        }

        auto diff = nlohmann::json::object();

        // Compare registers
        auto reg_diff = nlohmann::json::object();
        if (snapshot1.contains("registers") && snapshot2.contains("registers")) {
            auto& regs1 = snapshot1["registers"];
            auto& regs2 = snapshot2["registers"];
            for (auto& [key, val] : regs1.items()) {
                if (regs2.contains(key) && regs2[key] != val) {
                    reg_diff[key] = {{"before", val}, {"after", regs2[key]}};
                }
            }
        }
        if (!reg_diff.empty()) {
            diff["register_changes"] = reg_diff;
        }

        // Compare memory region count
        if (snapshot1.contains("memory_regions") && snapshot2.contains("memory_regions")) {
            auto count1 = snapshot1["memory_regions"].size();
            auto count2 = snapshot2["memory_regions"].size();
            if (count1 != count2) {
                diff["memory_region_count_change"] = {
                    {"before", count1},
                    {"after", count2}
                };
            }
        }

        // Compare breakpoints
        if (snapshot1.contains("breakpoints") && snapshot2.contains("breakpoints")) {
            auto count1 = snapshot1["breakpoints"].size();
            auto count2 = snapshot2["breakpoints"].size();
            if (count1 != count2) {
                diff["breakpoint_count_change"] = {
                    {"before", count1},
                    {"after", count2}
                };
            }
        }

        return s_http_response::ok({
            {"has_changes", !diff.empty()},
            {"diff", diff}
        });
    });

    // POST /api/patches/auto - Apply common patch patterns automatically
    router.post("/api/patches/auto", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return s_http_response::bad_request("Invalid JSON body");
        }

        auto patch_type = body.value("type", std::string());
        auto address_str = body.value("address", std::string());

        if (patch_type.empty() || address_str.empty()) {
            return s_http_response::bad_request("Missing 'type' and/or 'address' fields");
        }

        auto address = bridge.eval_expression(address_str);
        std::string cmd;
        std::string description;

        if (patch_type == "nop") {
            // NOP a single instruction (1 byte 0x90)
            cmd = "Fill " + format_utils::format_address(address) + ", 1, 90";
            description = "NOP instruction at " + address_str;
        } else if (patch_type == "nop_range") {
            auto size = body.value("size", std::string("1"));
            cmd = "Fill " + format_utils::format_address(address) + ", " + size + ", 90";
            description = "NOP " + size + " bytes at " + address_str;
        } else if (patch_type == "jmp") {
            auto target = body.value("target", std::string());
            if (target.empty()) {
                return s_http_response::bad_request("Missing 'target' for jmp patch");
            }
            // x64dbg: asm <address>, "jmp <target>"
            cmd = "asm " + address_str + ", \"jmp " + target + "\"";
            description = "JMP from " + address_str + " to " + target;
        } else if (patch_type == "je" || patch_type == "jz") {
            auto target = body.value("target", std::string());
            if (target.empty()) {
                return s_http_response::bad_request("Missing 'target' for conditional jump patch");
            }
            cmd = "asm " + address_str + ", \"je " + target + "\"";
            description = "JE from " + address_str + " to " + target;
        } else if (patch_type == "jne" || patch_type == "jnz") {
            auto target = body.value("target", std::string());
            if (target.empty()) {
                return s_http_response::bad_request("Missing 'target' for conditional jump patch");
            }
            cmd = "asm " + address_str + ", \"jne " + target + "\"";
            description = "JNE from " + address_str + " to " + target;
        } else if (patch_type == "set_byte") {
            auto value = body.value("value", std::string());
            if (value.empty()) {
                return s_http_response::bad_request("Missing 'value' for set_byte patch");
            }
            cmd = "Fill " + format_utils::format_address(address) + ", 1, " + value;
            description = "Set byte at " + address_str + " to 0x" + value;
        } else if (patch_type == "ret") {
            cmd = "asm " + address_str + ", \"ret\"";
            description = "Replace with RET at " + address_str;
        } else if (patch_type == "ret_0") {
            cmd = "asm " + address_str + ", \"xor eax, eax\" ; asm " + address_str + "+2, \"ret\"";
            description = "Replace with xor eax,eax; ret at " + address_str;
        } else if (patch_type == "invert_jz" || patch_type == "invert_jnz") {
            // Byte-level opcode inversion: swap JZ <-> JNZ without relying on the assembler.
            // This works even if the address was previously modified by other patches.
            // Short jumps:  JZ=0x74, JNZ=0x75
            // Near jumps:   JZ=0x0F84, JNZ=0x0F85
            auto original = bridge.read_memory(address, 2);
            if (!original.has_value() || original.value().size() < 2) {
                return s_http_response::internal_error("Failed to read bytes at " + address_str);
            }

            auto bytes = original.value();
            std::vector<uint8_t> patch_bytes;
            std::string detected_type;

            if (bytes[0] == 0x74) {
                // Short JZ (0x74)
                if (patch_type != "invert_jz") {
                    return s_http_response::bad_request(
                        "Instruction at " + address_str + " is short JZ (0x74), but requested "
                        + patch_type + ". Use invert_jz to invert a JZ instruction.");
                }
                patch_bytes = {0x75};
                detected_type = "short JZ (0x74)";
            } else if (bytes[0] == 0x75) {
                // Short JNZ (0x75)
                if (patch_type != "invert_jnz") {
                    return s_http_response::bad_request(
                        "Instruction at " + address_str + " is short JNZ (0x75), but requested "
                        + patch_type + ". Use invert_jnz to invert a JNZ instruction.");
                }
                patch_bytes = {0x74};
                detected_type = "short JNZ (0x75)";
            } else if (bytes[0] == 0x0F && bytes[1] == 0x84) {
                // Near JZ (0x0F84)
                if (patch_type != "invert_jz") {
                    return s_http_response::bad_request(
                        "Instruction at " + address_str + " is near JZ (0x0F84), but requested "
                        + patch_type + ". Use invert_jz to invert a JZ instruction.");
                }
                patch_bytes = {0x0F, 0x85};
                detected_type = "near JZ (0x0F84)";
            } else if (bytes[0] == 0x0F && bytes[1] == 0x85) {
                // Near JNZ (0x0F85)
                if (patch_type != "invert_jnz") {
                    return s_http_response::bad_request(
                        "Instruction at " + address_str + " is near JNZ (0x0F85), but requested "
                        + patch_type + ". Use invert_jnz to invert a JNZ instruction.");
                }
                patch_bytes = {0x0F, 0x84};
                detected_type = "near JNZ (0x0F85)";
            } else {
                return s_http_response::bad_request(
                    "No JZ/JNZ instruction found at " + address_str +
                    ". First bytes: " + format_utils::format_bytes_hex(bytes.data(), bytes.size()));
            }

            auto result = bridge.write_memory(address, patch_bytes);
            if (!result.has_value()) {
                return s_http_response::internal_error(
                    "Failed to invert " + detected_type + " at " + address_str + ": " + result.error());
            }

            // Build original hex string for response
            char hex_buf[8];
            if (patch_bytes.size() == 1) {
                snprintf(hex_buf, sizeof(hex_buf), "%02X", bytes[0]);
            } else {
                snprintf(hex_buf, sizeof(hex_buf), "%02X%02X", bytes[0], bytes[1]);
            }
            std::string original_hex = hex_buf;

            auto new_bytes = bridge.read_memory(address, patch_bytes.size());
            std::string new_hex;
            if (new_bytes.has_value() && !new_bytes.value().empty()) {
                char new_hex_buf[8];
                if (new_bytes.value().size() == 1) {
                    snprintf(new_hex_buf, sizeof(new_hex_buf), "%02X", new_bytes.value()[0]);
                } else {
                    snprintf(new_hex_buf, sizeof(new_hex_buf), "%02X%02X", new_bytes.value()[0], new_bytes.value()[1]);
                }
                new_hex = new_hex_buf;
            }

            return s_http_response::ok({
                {"success", true},
                {"type", patch_type},
                {"address", address_str},
                {"description", std::string(patch_type == "invert_jz" ? "Invert JZ to JNZ" : "Invert JNZ to JZ") + " at " + address_str + " (byte-level opcode swap)"},
                {"detected_type", detected_type},
                {"original_bytes", original_hex},
                {"new_bytes", new_hex}
            });
        } else {
            return s_http_response::bad_request("Unknown patch type: " + patch_type +
                ". Supported: nop, nop_range, jmp, je, jne, set_byte, ret, ret_0, invert_jz, invert_jnz");
        }

        // Read original bytes first for backup
        auto original = bridge.read_memory(address, 1);
        std::string original_hex;
        if (original.has_value() && !original.value().empty()) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X", original.value()[0]);
            original_hex = hex;
        }

        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({
            {"success", success},
            {"type", patch_type},
            {"address", address_str},
            {"description", description},
            {"command", cmd},
            {"original_byte", original_hex}
        });
    });

    // POST /api/annotations/batch - Set multiple labels/comments in one call
    router.post("/api/annotations/batch", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("annotations")) {
            return s_http_response::bad_request("Missing 'annotations' array");
        }

        auto annotations = body["annotations"];
        if (!annotations.is_array()) {
            return s_http_response::bad_request("'annotations' must be an array");
        }

        auto results = nlohmann::json::array();
        int success_count = 0;
        int fail_count = 0;

        for (const auto& ann : annotations) {
            auto type = ann.value("type", std::string());
            auto address_str = ann.value("address", std::string());
            auto text = ann.value("text", std::string());

            if (type.empty() || address_str.empty()) {
                results.push_back({
                    {"success", false},
                    {"error", "Missing 'type' and/or 'address'"},
                    {"annotation", ann}
                });
                fail_count++;
                continue;
            }

            auto address = bridge.eval_expression(address_str);
            bool success = false;

            if (type == "label") {
                success = bridge.set_label_at(address, text);
            } else if (type == "comment") {
                success = bridge.set_comment_at(address, text);
            } else if (type == "bookmark") {
                success = bridge.set_bookmark_at(address, true);
            } else {
                results.push_back({
                    {"success", false},
                    {"error", "Unknown type: " + type + ". Supported: label, comment, bookmark"},
                    {"annotation", ann}
                });
                fail_count++;
                continue;
            }

            if (success) success_count++;
            else fail_count++;

            results.push_back({
                {"success", success},
                {"type", type},
                {"address", address_str},
                {"text", text}
            });
        }

        return s_http_response::ok({
            {"total", annotations.size()},
            {"success_count", success_count},
            {"fail_count", fail_count},
            {"results", results}
        });
    });

    // POST /api/script/engine - Execute a multi-step script in one call
    router.post("/api/script/engine", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("steps")) {
            return s_http_response::bad_request("Missing 'steps' array");
        }

        auto steps = body["steps"];
        if (!steps.is_array()) {
            return s_http_response::bad_request("'steps' must be an array of command strings");
        }

        auto results = nlohmann::json::array();
        int success_count = 0;
        int fail_count = 0;

        for (const auto& step : steps) {
            auto cmd = step.get<std::string>();
            auto success = bridge.exec_command(cmd);

            // Try to get the result value
            auto result_val = bridge.eval_expression("$result");

            results.push_back({
                {"command", cmd},
                {"success", success},
                {"result", result_val}
            });

            if (success) success_count++;
            else fail_count++;
        }

        return s_http_response::ok({
            {"total", steps.size()},
            {"success_count", success_count},
            {"fail_count", fail_count},
            {"results", results}
        });
    });

    // POST /api/search/pattern_asm - Search for assembly instruction patterns
    router.post("/api/search/pattern_asm", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return s_http_response::bad_request("Invalid JSON body");
        }

        auto pattern_type = body.value("type", std::string());
        auto pattern = body.value("pattern", std::string());
        auto api_name = body.value("api", std::string());

        std::string cmd;

        if (pattern_type == "call_indirect") {
            // Find all indirect calls: call [reg+offset] or call reg
            cmd = "findasm \"call [";
            if (!pattern.empty()) cmd += pattern;
            else cmd += "e";
            cmd += "x*4]\"";
        } else if (pattern_type == "stack_access") {
            // Find stack accesses: mov eax, [ebp+arg_*]
            cmd = "findasm \"mov eax, [ebp+";
            if (!pattern.empty()) cmd += pattern;
            else cmd += "arg_0";
            cmd += "]\"";
        } else if (pattern_type == "api_call") {
            // Find calls to a specific API
            if (api_name.empty()) {
                return s_http_response::bad_request("Missing 'api' field for api_call pattern");
            }
            cmd = "findasm \"call " + api_name + "\"";
        } else if (pattern_type == "jmp_table") {
            // Find jump table patterns: jmp [reg*4+base]
            cmd = "findasm \"jmp [";
            if (!pattern.empty()) cmd += pattern;
            else cmd += "e";
            cmd += "x*4]\"";
        } else if (pattern_type == "custom") {
            if (pattern.empty()) {
                return s_http_response::bad_request("Missing 'pattern' field for custom pattern");
            }
            cmd = "findasm \"" + pattern + "\"";
        } else {
            return s_http_response::bad_request("Unknown pattern type: " + pattern_type +
                ". Supported: call_indirect, stack_access, api_call, jmp_table, custom");
        }

        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({
            {"success", success},
            {"type", pattern_type},
            {"command", cmd}
        });
    });

    // POST /api/workflow/execute - Execute a conditional multi-step workflow
    router.post("/api/workflow/execute", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("steps")) {
            return s_http_response::bad_request("Missing 'steps' array");
        }

        auto steps = body["steps"];
        if (!steps.is_array()) {
            return s_http_response::bad_request("'steps' must be an array");
        }

        auto workflow_results = nlohmann::json::array();
        bool abort = false;

        for (const auto& step : steps) {
            if (abort) {
                workflow_results.push_back({
                    {"skipped", true},
                    {"reason", "Previous step aborted workflow"}
                });
                continue;
            }

            // Check for conditional step
            if (step.contains("condition")) {
                auto condition = step["condition"].get<std::string>();
                auto condition_result = bridge.eval_expression(condition);

                if (condition_result == 0) {
                    workflow_results.push_back({
                        {"condition", condition},
                        {"evaluated", false},
                        {"skipped", true}
                    });
                    continue;
                }

                // Execute the 'then' steps
                if (step.contains("then") && step["then"].is_array()) {
                    for (const auto& then_step : step["then"]) {
                        auto cmd = then_step.get<std::string>();
                        auto success = bridge.exec_command(cmd);
                        workflow_results.push_back({
                            {"command", cmd},
                            {"success", success}
                        });
                        if (!success) abort = true;
                    }
                }
                continue;
            }

            // Regular step: tool + params
            if (step.contains("command")) {
                auto cmd = step["command"].get<std::string>();
                auto success = bridge.exec_command(cmd);
                workflow_results.push_back({
                    {"command", cmd},
                    {"success", success}
                });
                if (!success) abort = true;
            }
        }

        return s_http_response::ok({
            {"total_steps", steps.size()},
            {"executed", workflow_results.size()},
            {"aborted", abort},
            {"results", workflow_results}
        });
    });

    // POST /api/misc/bpgoto - Configure breakpoint to redirect execution
    router.post("/api/misc/bpgoto", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("target")) {
            return s_http_response::bad_request("Missing 'address' and/or 'target' fields");
        }

        auto address = body["address"].get<std::string>();
        auto target = body["target"].get<std::string>();
        auto cmd = "bpgoto " + address + ", " + target;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/chd - Change current directory
    router.post("/api/misc/chd", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("path")) {
            return s_http_response::bad_request("Missing 'path' field");
        }

        auto path = body["path"].get<std::string>();
        auto cmd = "chd \"" + path + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/memmapdump - Follow address in memory map
    router.post("/api/misc/memmapdump", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto address = body.value("address", std::string("cip"));

        auto cmd = "memmapdump " + address;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/sdump - Stack dump at position
    router.post("/api/misc/sdump", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto address = body.value("address", std::string("csp"));

        auto cmd = "sdump " + address;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/set_freezestack - Set stack freeze state
    router.post("/api/misc/set_freezestack", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto freeze = body.value("freeze", std::string("1"));

        auto cmd = "setfreezestack " + freeze;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/database/clear - Clear database from memory
    router.post("/api/database/clear", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto success = bridge.exec_command("cleardb");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/labels/delete - Delete a label
    router.post("/api/labels/delete", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address")) {
            return s_http_response::bad_request("Missing 'address' field");
        }

        auto address = body["address"].get<std::string>();
        auto cmd = "labeldel " + address;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/variables/new - Declare a new variable
    router.post("/api/variables/new", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("value")) {
            return s_http_response::bad_request("Missing 'name' and/or 'value' fields");
        }

        auto name = body["name"].get<std::string>();
        auto value = body["value"].get<std::string>();
        auto cmd = "var " + name + ", " + value;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/variables/delete - Delete a variable
    router.post("/api/variables/delete", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) {
            return s_http_response::bad_request("Missing 'name' field");
        }

        auto name = body["name"].get<std::string>();
        auto cmd = "vardel " + name;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // GET /api/variables/list - List all variables
    router.get("/api/variables/list", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("varlist");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/traceexecute - Mark address as traced
    router.post("/api/misc/traceexecute", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto address = body.value("address", std::string("cip"));

        auto cmd = "traceexecute " + address;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/command/script_cmd - Execute command in script context
    router.post("/api/command/script_cmd", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("command")) {
            return s_http_response::bad_request("Missing 'command' field");
        }

        auto command = body["command"].get<std::string>();
        auto cmd = "scriptcmd " + command;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/command/script_dll - Execute a script DLL
    router.post("/api/command/script_dll", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("path")) {
            return s_http_response::bad_request("Missing 'path' field");
        }

        auto path = body["path"].get<std::string>();
        auto cmd = "scriptdll \"" + path + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/search/reffind_range - Find references to a range of values
    router.post("/api/search/reffind_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) {
            return s_http_response::bad_request("Missing 'start' and/or 'end' fields");
        }

        auto start = body["start"].get<std::string>();
        auto end = body["end"].get<std::string>();
        auto cmd = "reffindrange " + start + ", " + end;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/search/set_max_results - Set maximum search results
    router.post("/api/search/set_max_results", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto max_results = body.value("max_results", std::string("1000"));

        auto cmd = "setmaxfindresult " + max_results;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/refinit - Initialize reference view
    router.post("/api/misc/refinit", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("refinit");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/refadd - Add entry to reference view
    router.post("/api/misc/refadd", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("text")) {
            return s_http_response::bad_request("Missing 'address' and/or 'text' fields");
        }

        auto address = body["address"].get<std::string>();
        auto text = body["text"].get<std::string>();
        auto cmd = "refadd " + address + ", \"" + text + "\"";
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // GET /api/misc/refget - Get reference at address
    router.get("/api/misc/refget", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address = req.get_query("address");
        if (address.empty()) {
            return s_http_response::bad_request("Missing 'address' query parameter");
        }

        auto cmd = "refget " + address;
        auto success = bridge.exec_command(cmd);

        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/hide_debugger - Hide debugger from detection
    router.post("/api/misc/hide_debugger", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("HideDebugger");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/debug/step_system - Step into system modules
    router.post("/api/debug/step_system", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }
        auto success = bridge.exec_command("StepSystem");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/debug/step_user - Step into user modules only
    router.post("/api/debug/step_user", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }
        auto success = bridge.exec_command("StepUser");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/clear_log - Clear the log window
    router.post("/api/misc/clear_log", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("cls");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/disable_log - Disable log output
    router.post("/api/misc/disable_log", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("LogDisable");
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/misc/enable_log - Enable log output
    router.post("/api/misc/enable_log", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        auto success = bridge.exec_command("LogEnable");
        return s_http_response::ok({{"success", success}});
    });

    // GET /api/misc/get_reloc_size - Get relocation table size
    router.get("/api/misc/get_reloc_size", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto module = req.get_query("module", "0");
        auto cmd = "GetRelocSize " + module;
        auto success = bridge.exec_command(cmd);
        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/set_bp_options - Set default breakpoint type
    router.post("/api/misc/set_bp_options", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto bp_type = body.value("type", std::string("0"));
        auto cmd = "SetBPXOptions " + bp_type;
        auto success = bridge.exec_command(cmd);
        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/debug/continue - Set debugger continue status
    router.post("/api/debug/continue", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto status = body.value("status", std::string("HandleException"));
        auto cmd = "DebugContinue " + status;
        auto success = bridge.exec_command(cmd);
        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/enable_privilege - Enable a privilege
    router.post("/api/misc/enable_privilege", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto privilege = body.value("privilege", std::string("SeDebugPrivilege"));
        auto cmd = "EnablePrivilege " + privilege;
        auto success = bridge.exec_command(cmd);
        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/disable_privilege - Disable a privilege
    router.post("/api/misc/disable_privilege", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto privilege = body.value("privilege", std::string("SeDebugPrivilege"));
        auto cmd = "DisablePrivilege " + privilege;
        auto success = bridge.exec_command(cmd);
        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // GET /api/misc/get_privilege_state - Query privilege state
    router.get("/api/misc/get_privilege_state", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto privilege = req.get_query("privilege", "SeDebugPrivilege");
        auto cmd = "GetPrivilegeState " + privilege;
        auto success = bridge.exec_command(cmd);
        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/misc/fold_disassembly - Fold disassembly range
    router.post("/api/misc/fold_disassembly", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) {
            return s_http_response::bad_request("Missing 'start' and/or 'end' fields");
        }
        auto start = body["start"].get<std::string>();
        auto end = body["end"].get<std::string>();
        auto cmd = "FoldDisassembly " + start + ", " + end;
        auto success = bridge.exec_command(cmd);
        return s_http_response::ok({{"success", success}, {"command", cmd}});
    });

    // POST /api/breakpoints/set_memory_range - Set memory BP on specific range
    router.post("/api/breakpoints/set_memory_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) { return s_http_response::conflict("Debugger must be paused"); }
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("size")) { return s_http_response::bad_request("Missing 'address' and/or 'size'"); }
        auto cmd = "SetMemoryRangeBPX " + body["address"].get<std::string>() + ", " + body["size"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/breakpoints/librarian_set - Set DLL load/unload breakpoint
    router.post("/api/breakpoints/librarian_set", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "LibrarianSetBreakpoint " + body["name"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/breakpoints/librarian_remove - Remove DLL breakpoint
    router.post("/api/breakpoints/librarian_remove", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "LibrarianRemoveBreakpoint " + body["name"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/breakpoints/librarian_enable - Enable DLL breakpoint
    router.post("/api/breakpoints/librarian_enable", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "LibrarianEnableBreakpoint " + body["name"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/breakpoints/librarian_disable - Disable DLL breakpoint
    router.post("/api/breakpoints/librarian_disable", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "LibrarianDisableBreakpoint " + body["name"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // GET /api/breakpoints/hitcount - Get breakpoint hit count
    router.get("/api/breakpoints/hitcount", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto type = req.get_query("type", "software");
        auto address = req.get_query("address", "");
        std::string cmd;
        if (type == "exception") cmd = "GetExceptionBreakpointHitCount";
        else if (type == "hardware") cmd = "GetHardwareBreakpointHitCount";
        else if (type == "librarian") cmd = "GetLibrarianBreakpointHitCount";
        else if (type == "memory") cmd = "GetMemoryBreakpointHitCount";
        else cmd = "GetBreakpointHitCount";
        if (!address.empty()) cmd += " " + address;
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/breakpoints/reset_hitcount_by_type - Reset hit count by BP type
    router.post("/api/breakpoints/reset_hitcount_by_type", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto type = body.value("type", std::string("software"));
        auto address = body.value("address", std::string(""));
        std::string cmd;
        if (type == "exception") cmd = "ResetExceptionBreakpointHitCount";
        else if (type == "hardware") cmd = "ResetHardwareBreakpointHitCount";
        else if (type == "librarian") cmd = "ResetLibrarianBreakpointHitCount";
        else if (type == "memory") cmd = "ResetMemoryBreakpointHitCount";
        else cmd = "ResetBreakpointHitCount";
        if (!address.empty()) cmd += " " + address;
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/misc/add_favourite_command - Add command to favourites menu
    router.post("/api/misc/add_favourite_command", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("command")) { return s_http_response::bad_request("Missing 'command'"); }
        auto cmd = "AddFavouriteCommand \"" + body["command"].get<std::string>() + "\"";
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/misc/add_favourite_tool - Add tool to favourites menu
    router.post("/api/misc/add_favourite_tool", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("path") || !body.contains("description")) { return s_http_response::bad_request("Missing 'path' and/or 'description'"); }
        auto cmd = "AddFavouriteTool \"" + body["path"].get<std::string>() + "\", \"" + body["description"].get<std::string>() + "\"";
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/misc/add_favourite_shortcut - Set favourite tool shortcut
    router.post("/api/misc/add_favourite_shortcut", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("shortcut") || !body.contains("description")) { return s_http_response::bad_request("Missing 'shortcut' and/or 'description'"); }
        auto cmd = "SetFavouriteToolShortcut \"" + body["shortcut"].get<std::string>() + "\", \"" + body["description"].get<std::string>() + "\"";
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/misc/start_scylla - Start Scylla IAT reconstruction
    router.post("/api/misc/start_scylla", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        return s_http_response::ok({{"success", bridge.exec_command("scylla")}});
    });

    // POST /api/types/set_data - Set data type at address
    router.post("/api/types/set_data", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("type") || !body.contains("address")) { return s_http_response::bad_request("Missing 'type' and/or 'address'"); }
        auto type = body["type"].get<std::string>();
        auto address = body["address"].get<std::string>();
        static const std::unordered_map<std::string, std::string> data_cmds = {
            {"ascii", "DataAscii"}, {"byte", "DataByte"}, {"code", "DataCode"}, {"double", "DataDouble"},
            {"dword", "DataDword"}, {"float", "DataFloat"}, {"fword", "DataFword"}, {"junk", "DataJunk"},
            {"longdouble", "DataLongdouble"}, {"middle", "DataMiddle"}, {"mmword", "DataMmword"},
            {"oword", "DataOword"}, {"qword", "DataQword"}, {"tbyte", "DataTbyte"},
            {"unicode", "DataUnicode"}, {"unknown", "DataUnknown"}, {"word", "DataWord"},
            {"xmmword", "DataXmmword"}, {"ymmword", "DataYmmword"}
        };
        auto it = data_cmds.find(type);
        if (it == data_cmds.end()) {
            std::string valid;
            for (const auto& [k, v] : data_cmds) { if (!valid.empty()) valid += ", "; valid += k; }
            return s_http_response::bad_request("Unknown type: " + type + ". Valid: " + valid);
        }
        auto cmd = it->second + " " + address;
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"type", type}, {"command", cmd}});
    });

    // POST /api/types/add_struct - Add a new struct type
    router.post("/api/types/add_struct", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "AddStruct " + body["name"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/add_union - Add a new union type
    router.post("/api/types/add_union", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "AddUnion " + body["name"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/add_type - Add a type alias
    router.post("/api/types/add_type", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("type")) { return s_http_response::bad_request("Missing 'name' and/or 'type'"); }
        auto cmd = "AddType " + body["name"].get<std::string>() + ", " + body["type"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/add_function - Add a function type
    router.post("/api/types/add_function", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("return_type") || !body.contains("args")) {
            return s_http_response::bad_request("Missing 'name', 'return_type', and/or 'args'");
        }
        auto cmd = "AddFunction " + body["name"].get<std::string>() + ", " + body["return_type"].get<std::string>() + ", " + body["args"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/add_arg - Add argument to function
    router.post("/api/types/add_arg", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("type")) { return s_http_response::bad_request("Missing 'name' and/or 'type'"); }
        auto cmd = "AddArg " + body["name"].get<std::string>() + ", " + body["type"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/add_member - Add member to struct/union
    router.post("/api/types/add_member", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("type")) { return s_http_response::bad_request("Missing 'name' and/or 'type'"); }
        auto cmd = "AddMember " + body["name"].get<std::string>() + ", " + body["type"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/append_arg - Append argument to last function
    router.post("/api/types/append_arg", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("type")) { return s_http_response::bad_request("Missing 'name' and/or 'type'"); }
        auto cmd = "AppendArg " + body["name"].get<std::string>() + ", " + body["type"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/append_member - Append member to last struct/union
    router.post("/api/types/append_member", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("type")) { return s_http_response::bad_request("Missing 'name' and/or 'type'"); }
        auto cmd = "AppendMember " + body["name"].get<std::string>() + ", " + body["type"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/clear - Clear all types
    router.post("/api/types/clear", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        return s_http_response::ok({{"success", bridge.exec_command("ClearTypes")}});
    });

    // GET /api/types/enum - Enumerate all types
    router.get("/api/types/enum", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        return s_http_response::ok({{"success", bridge.exec_command("EnumTypes")}});
    });

    // POST /api/types/load - Load types from JSON file
    router.post("/api/types/load", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("path")) { return s_http_response::bad_request("Missing 'path'"); }
        auto cmd = "LoadTypes \"" + body["path"].get<std::string>() + "\"";
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/parse - Parse and load types from header file
    router.post("/api/types/parse", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("path")) { return s_http_response::bad_request("Missing 'path'"); }
        auto cmd = "ParseTypes \"" + body["path"].get<std::string>() + "\"";
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/types/remove - Remove a type
    router.post("/api/types/remove", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "RemoveType " + body["name"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // GET /api/types/sizeof - Get size of a type
    router.get("/api/types/sizeof", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto name = req.get_query("name");
        if (name.empty()) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "SizeofType " + name;
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // GET /api/types/display - Display a type
    router.get("/api/types/display", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto name = req.get_query("name");
        if (name.empty()) { return s_http_response::bad_request("Missing 'name'"); }
        auto cmd = "VisitType " + name;
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/watch/add - Add a watch item
    router.post("/api/watch/add", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("expression")) { return s_http_response::bad_request("Missing 'expression'"); }
        auto name = body.value("name", std::string());
        auto cmd = "AddWatch " + body["expression"].get<std::string>();
        if (!name.empty()) cmd += ", " + name;
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/watch/delete - Delete a watch item
    router.post("/api/watch/delete", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("id")) { return s_http_response::bad_request("Missing 'id'"); }
        auto cmd = "DelWatch " + body["id"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/watch/set_watchdog - Set watchdog mode
    router.post("/api/watch/set_watchdog", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("id") || !body.contains("mode")) { return s_http_response::bad_request("Missing 'id' and/or 'mode'"); }
        auto cmd = "SetWatchdog " + body["id"].get<std::string>() + ", " + body["mode"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/watch/set_expression - Change watch expression
    router.post("/api/watch/set_expression", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("id") || !body.contains("expression")) { return s_http_response::bad_request("Missing 'id' and/or 'expression'"); }
        auto cmd = "SetWatchExpression " + body["id"].get<std::string>() + ", " + body["expression"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // POST /api/watch/set_name - Rename a watch item
    router.post("/api/watch/set_name", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("id") || !body.contains("name")) { return s_http_response::bad_request("Missing 'id' and/or 'name'"); }
        auto cmd = "SetWatchName " + body["id"].get<std::string>() + ", " + body["name"].get<std::string>();
        return s_http_response::ok({{"success", bridge.exec_command(cmd)}, {"command", cmd}});
    });

    // GET /api/watch/check_watchdog - Check all watchdogs
    router.get("/api/watch/check_watchdog", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        return s_http_response::ok({{"success", bridge.exec_command("CheckWatchdog")}});
    });

    // POST /api/dbg/set_value - Set register/variable value (DbgValSetScalar)
    router.post("/api/dbg/set_value", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("value")) { return s_http_response::bad_request("Missing 'name' and/or 'value'"); }
        auto name = body["name"].get<std::string>();
        auto value_str = body["value"].get<std::string>();
        auto value = bridge.eval_expression(value_str);
        auto success = DbgValSetScalar(name.c_str(), value);
        return s_http_response::ok({{"success", success}, {"name", name}, {"value", value}});
    });

    // POST /api/dbg/set_buffer - Set value from raw buffer (DbgValSetBuffer)
    router.post("/api/dbg/set_buffer", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name") || !body.contains("data")) { return s_http_response::bad_request("Missing 'name' and/or 'data'"); }
        auto name = body["name"].get<std::string>();
        auto data_str = body["data"].get<std::string>();
        // Parse hex string to bytes
        auto bytes = format_utils::parse_hex_bytes(data_str);
        auto success = DbgValSetBuffer(name.c_str(), bytes.data(), bytes.size());
        return s_http_response::ok({{"success", success}, {"name", name}});
    });

    // GET /api/dbg/xref_count - Get xref count at address
    router.get("/api/dbg/xref_count", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address", "cip");
        auto address = bridge.eval_expression(address_str);
        auto count = DbgGetXrefCountAt(address);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"count", count}});
    });

    // GET /api/dbg/xref_type - Get xref type at address
    router.get("/api/dbg/xref_type", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address", "cip");
        auto address = bridge.eval_expression(address_str);
        auto type = DbgGetXrefTypeAt(address);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"type", static_cast<int>(type)}});
    });

    // GET /api/dbg/time_wasted - Get time wasted counter
    router.get("/api/dbg/time_wasted", [](const s_http_request&) -> s_http_response {
        auto counter = DbgGetTimeWastedCounter();
        return s_http_response::ok({{"counter", counter}});
    });

    // GET /api/dbg/watch_list - Get watch list
    router.get("/api/dbg/watch_list", [](const s_http_request&) -> s_http_response {
        BridgeList<WATCHINFO> list;
        DbgGetWatchList(&list);
        return s_http_response::ok({{"count", list.Count()}});
    });

    // GET /api/dbg/is_run_locked - Check if run is locked
    router.get("/api/dbg/is_run_locked", [](const s_http_request&) -> s_http_response {
        return s_http_response::ok({{"locked", DbgIsRunLocked()}});
    });

    // GET /api/dbg/is_valid_expression - Check if expression is valid
    router.get("/api/dbg/is_valid_expression", [](const s_http_request& req) -> s_http_response {
        auto expr = req.get_query("expression");
        if (expr.empty()) { return s_http_response::bad_request("Missing 'expression'"); }
        return s_http_response::ok({{"expression", expr}, {"valid", DbgIsValidExpression(expr.c_str())}});
    });

    // GET /api/dbg/is_valid_read_ptr - Check if pointer is readable
    router.get("/api/dbg/is_valid_read_ptr", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"valid", DbgMemIsValidReadPtr(address)}});
    });

    // GET /api/dbg/bpx_type - Get breakpoint type at address
    router.get("/api/dbg/bpx_type", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"type", static_cast<int>(DbgGetBpxTypeAt(address))}});
    });

    // GET /api/dbg/is_bp_disabled - Check if BP is disabled
    router.get("/api/dbg/is_bp_disabled", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"disabled", DbgIsBpDisabled(address)}});
    });

    // GET /api/dbg/encode_type - Get encode type at address
    router.get("/api/dbg/encode_type", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        // DbgGetEncodeTypeAt needs a size parameter; pass 1 as default
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"type", static_cast<int>(DbgGetEncodeTypeAt(address, 1))}});
    });

    // POST /api/dbg/set_encode_type - Set encode type at address
    router.post("/api/dbg/set_encode_type", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("type")) { return s_http_response::bad_request("Missing 'address' and/or 'type'"); }
        auto address = bridge.eval_expression(body["address"].get<std::string>());
        auto type_value = bridge.eval_expression(body["type"].get<std::string>());
        // DbgSetEncodeType needs a size parameter; pass 1 as default
        return s_http_response::ok({{"success", DbgSetEncodeType(address, 1, static_cast<ENCODETYPE>(type_value))}, {"address", body["address"].get<std::string>()}});
    });

    // GET /api/dbg/encode_size - Get encode size at address
    router.get("/api/dbg/encode_size", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        // DbgGetEncodeSizeAt needs a codesize parameter; pass 1 as default
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"size", DbgGetEncodeSizeAt(address, 1)}});
    });

    // GET /api/dbg/arg_type - Get argument type at address
    router.get("/api/dbg/arg_type", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"type", static_cast<int>(DbgGetArgTypeAt(address))}});
    });

    // GET /api/dbg/bookmark_at - Get bookmark at address
    router.get("/api/dbg/bookmark_at", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"bookmarked", DbgGetBookmarkAt(address)}});
    });

    // GET /api/dbg/comment_at - Get comment at address
    router.get("/api/dbg/comment_at", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        char comment[MAX_COMMENT_SIZE] = {};
        auto found = DbgGetCommentAt(address, comment);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"found", found}, {"comment", found ? std::string(comment) : ""}});
    });

    // GET /api/dbg/label_at - Get label at address
    router.get("/api/dbg/label_at", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        char label[MAX_LABEL_SIZE] = {};
        auto found = DbgGetLabelAt(address, SEG_DEFAULT, label);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"found", found}, {"label", found ? std::string(label) : ""}});
    });

    // GET /api/dbg/loop_type - Get loop type at address
    router.get("/api/dbg/loop_type", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        // DbgGetLoopTypeAt needs a depth parameter; default 0
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"type", static_cast<int>(DbgGetLoopTypeAt(address, 0))}});
    });

    // POST /api/dbg/loop_add - Add a loop
    router.post("/api/dbg/loop_add", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        return s_http_response::ok({{"success", DbgLoopAdd(start, end)}});
    });

    // POST /api/dbg/loop_del - Delete a loop
    router.post("/api/dbg/loop_del", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address")) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(body["address"].get<std::string>());
        // DbgLoopDel needs a depth parameter; default 0
        return s_http_response::ok({{"success", DbgLoopDel(0, address)}});
    });

    // GET /api/dbg/loop_overlaps - Check loop overlap
    router.get("/api/dbg/loop_overlaps", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto start_str = req.get_query("start");
        auto end_str = req.get_query("end");
        if (start_str.empty() || end_str.empty()) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(start_str);
        auto end = bridge.eval_expression(end_str);
        // DbgLoopOverlaps needs a depth parameter; default 0
        return s_http_response::ok({{"overlaps", DbgLoopOverlaps(0, start, end)}});
    });

    // GET /api/dbg/function_overlaps - Check function overlap
    router.get("/api/dbg/function_overlaps", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto start_str = req.get_query("start");
        auto end_str = req.get_query("end");
        if (start_str.empty() || end_str.empty()) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(start_str);
        auto end = bridge.eval_expression(end_str);
        return s_http_response::ok({{"overlaps", DbgFunctionOverlaps(start, end)}});
    });

    // GET /api/dbg/argument_overlaps - Check argument overlap
    router.get("/api/dbg/argument_overlaps", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto start_str = req.get_query("start");
        auto end_str = req.get_query("end");
        if (start_str.empty() || end_str.empty()) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(start_str);
        auto end = bridge.eval_expression(end_str);
        return s_http_response::ok({{"overlaps", DbgArgumentOverlaps(start, end)}});
    });

    // GET /api/dbg/argument_get - Get argument boundaries
    router.get("/api/dbg/argument_get", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        duint start = 0, end = 0;
        auto found = DbgArgumentGet(address, &start, &end);
        return s_http_response::ok({{"found", found}, {"start", format_utils::format_address(start)}, {"end", format_utils::format_address(end)}});
    });

    // POST /api/dbg/clear_auto_bookmark_range - Clear auto bookmarks in range
    router.post("/api/dbg/clear_auto_bookmark_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        DbgClearAutoBookmarkRange(start, end);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/clear_auto_comment_range - Clear auto comments in range
    router.post("/api/dbg/clear_auto_comment_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        DbgClearAutoCommentRange(start, end);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/clear_auto_label_range - Clear auto labels in range
    router.post("/api/dbg/clear_auto_label_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        DbgClearAutoLabelRange(start, end);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/clear_auto_function_range - Clear auto functions in range
    router.post("/api/dbg/clear_auto_function_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        DbgClearAutoFunctionRange(start, end);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/clear_bookmark_range - Clear bookmarks in range
    router.post("/api/dbg/clear_bookmark_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        DbgClearBookmarkRange(start, end);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/clear_comment_range - Clear comments in range
    router.post("/api/dbg/clear_comment_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        DbgClearCommentRange(start, end);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/clear_label_range - Clear labels in range
    router.post("/api/dbg/clear_label_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        DbgClearLabelRange(start, end);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/set_auto_bookmark - Set auto bookmark
    router.post("/api/dbg/set_auto_bookmark", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address")) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(body["address"].get<std::string>());
        return s_http_response::ok({{"success", DbgSetAutoBookmarkAt(address)}});
    });

    // POST /api/dbg/set_auto_comment - Set auto comment
    router.post("/api/dbg/set_auto_comment", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("text")) { return s_http_response::bad_request("Missing 'address' and/or 'text'"); }
        auto address = bridge.eval_expression(body["address"].get<std::string>());
        auto text = body["text"].get<std::string>();
        return s_http_response::ok({{"success", DbgSetAutoCommentAt(address, text.c_str())}});
    });

    // POST /api/dbg/set_auto_label - Set auto label
    router.post("/api/dbg/set_auto_label", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("text")) { return s_http_response::bad_request("Missing 'address' and/or 'text'"); }
        auto address = bridge.eval_expression(body["address"].get<std::string>());
        auto text = body["text"].get<std::string>();
        return s_http_response::ok({{"success", DbgSetAutoLabelAt(address, text.c_str())}});
    });

    // POST /api/dbg/set_auto_function - Set auto function
    router.post("/api/dbg/set_auto_function", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        return s_http_response::ok({{"success", DbgSetAutoFunctionAt(start, end)}});
    });

    // POST /api/dbg/del_encode_type_range - Delete encode types in range
    router.post("/api/dbg/del_encode_type_range", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start") || !body.contains("end")) { return s_http_response::bad_request("Missing 'start' and/or 'end'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        auto end = bridge.eval_expression(body["end"].get<std::string>());
        DbgDelEncodeTypeRange(start, end);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/del_encode_type_segment - Delete encode type segment
    router.post("/api/dbg/del_encode_type_segment", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("start")) { return s_http_response::bad_request("Missing 'start'"); }
        auto start = bridge.eval_expression(body["start"].get<std::string>());
        // DbgDelEncodeTypeSegment takes only a single start address
        DbgDelEncodeTypeSegment(start);
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/exit - Exit debugger
    router.post("/api/dbg/exit", [](const s_http_request&) -> s_http_response {
        DbgExit();
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/init - Initialize debugger
    router.post("/api/dbg/init", [](const s_http_request&) -> s_http_response {
        DbgInit();
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/dbg/settings_updated - Notify settings changed
    router.post("/api/dbg/settings_updated", [](const s_http_request&) -> s_http_response {
        DbgSettingsUpdated();
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/plugin/logprintf - Write formatted log
    router.post("/api/plugin/logprintf", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("text")) { return s_http_response::bad_request("Missing 'text'"); }
        auto text = body["text"].get<std::string>();
        _plugin_logprintf("[MCP] %s\n", text.c_str());
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/plugin/debug_pause - Force debugger pause
    router.post("/api/plugin/debug_pause", [](const s_http_request&) -> s_http_response {
        _plugin_debugpause();
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/plugin/debug_skip_exceptions - Skip all exceptions
    router.post("/api/plugin/debug_skip_exceptions", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        auto skip = body.value("skip", true);
        _plugin_debugskipexceptions(skip);
        return s_http_response::ok({{"success", true}, {"skip", skip}});
    });

    // POST /api/plugin/wait_until_paused - Wait for debugger to pause
    router.post("/api/plugin/wait_until_paused", [](const s_http_request&) -> s_http_response {
        auto success = _plugin_waituntilpaused();
        return s_http_response::ok({{"success", success}});
    });

    // POST /api/plugin/hash - Hash data
    router.post("/api/plugin/hash", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("data")) { return s_http_response::bad_request("Missing 'data'"); }
        auto data_str = body["data"].get<std::string>();
        auto hash = _plugin_hash(data_str.c_str(), data_str.size());
        return s_http_response::ok({{"hash", hash}});
    });

    // GET /api/bridge/setting - Get x64dbg setting
    router.get("/api/bridge/setting", [](const s_http_request& req) -> s_http_response {
        auto section = req.get_query("section");
        auto key = req.get_query("key");
        if (section.empty() || key.empty()) { return s_http_response::bad_request("Missing 'section' and/or 'key'"); }
        char value[MAX_SETTING_SIZE] = {};
        auto found = BridgeSettingGet(section.c_str(), key.c_str(), value);
        return s_http_response::ok({{"found", found}, {"section", section}, {"key", key}, {"value", found ? std::string(value) : ""}});
    });

    // POST /api/bridge/setting - Set x64dbg setting
    router.post("/api/bridge/setting", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("section") || !body.contains("key") || !body.contains("value")) { return s_http_response::bad_request("Missing 'section', 'key', and/or 'value'"); }
        return s_http_response::ok({{"success", BridgeSettingSet(body["section"].get<std::string>().c_str(), body["key"].get<std::string>().c_str(), body["value"].get<std::string>().c_str())}});
    });

    // GET /api/bridge/setting_uint - Get integer setting
    router.get("/api/bridge/setting_uint", [](const s_http_request& req) -> s_http_response {
        auto section = req.get_query("section");
        auto key = req.get_query("key");
        if (section.empty() || key.empty()) { return s_http_response::bad_request("Missing 'section' and/or 'key'"); }
        duint value = 0;
        auto found = BridgeSettingGetUint(section.c_str(), key.c_str(), &value);
        return s_http_response::ok({{"found", found}, {"section", section}, {"key", key}, {"value", value}});
    });

    // POST /api/bridge/setting_uint - Set integer setting
    router.post("/api/bridge/setting_uint", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("section") || !body.contains("key") || !body.contains("value")) { return s_http_response::bad_request("Missing 'section', 'key', and/or 'value'"); }
        return s_http_response::ok({{"success", BridgeSettingSetUint(body["section"].get<std::string>().c_str(), body["key"].get<std::string>().c_str(), static_cast<duint>(body["value"].get<uint64_t>()))}});
    });

    // POST /api/bridge/setting_flush - Flush settings to disk
    router.post("/api/bridge/setting_flush", [](const s_http_request&) -> s_http_response {
        return s_http_response::ok({{"success", BridgeSettingFlush()}});
    });

    // GET /api/bridge/nt_build - Get NT build number
    router.get("/api/bridge/nt_build", [](const s_http_request&) -> s_http_response {
        return s_http_response::ok({{"build", BridgeGetNtBuildNumber()}});
    });

    // GET /api/bridge/working_directory - Get original working directory
    router.get("/api/bridge/working_directory", [](const s_http_request&) -> s_http_response {
        auto dir = BridgeWorkingDirectory();
        return s_http_response::ok({{"directory", dir ? dir : L""}});
    });

    // GET /api/bridge/user_directory - Get x64dbg user directory
    router.get("/api/bridge/user_directory", [](const s_http_request&) -> s_http_response {
        auto dir = BridgeUserDirectory();
        return s_http_response::ok({{"directory", dir ? dir : L""}});
    });

    // GET /api/bridge/is_arm64 - Check if running under ARM64 emulation
    router.get("/api/bridge/is_arm64", [](const s_http_request&) -> s_http_response {
        return s_http_response::ok({{"arm64", BridgeIsARM64Emulated()}});
    });

    // POST /api/plugin/logputs - Write text to log
    router.post("/api/plugin/logputs", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("text")) { return s_http_response::bad_request("Missing 'text'"); }
        _plugin_logputs(body["text"].get<std::string>().c_str());
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/plugin/logprint - Write text to log
    router.post("/api/plugin/logprint", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("text")) { return s_http_response::bad_request("Missing 'text'"); }
        _plugin_logprint(body["text"].get<std::string>().c_str());
        return s_http_response::ok({{"success", true}});
    });

    // POST /api/plugin/load - Load a plugin by name
    router.post("/api/plugin/load", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        return s_http_response::ok({{"success", _plugin_load(body["name"].get<std::string>().c_str())}});
    });

    // POST /api/plugin/unload - Unload a plugin by name
    router.post("/api/plugin/unload", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) { return s_http_response::bad_request("Missing 'name'"); }
        return s_http_response::ok({{"success", _plugin_unload(body["name"].get<std::string>().c_str())}});
    });

    // GET /api/dbg/def_jit - Get default JIT debugger
    router.get("/api/dbg/def_jit", [](const s_http_request& req) -> s_http_response {
        auto is_x64_str = req.get_query("x64", "true");
        bool is_x64 = is_x64_str == "true" || is_x64_str == "1";
        (void)is_x64; // Parameter kept for API completeness
        char jit[MAX_SETTING_SIZE] = {};
        auto found = DbgFunctions()->GetDefJit(jit);
        return s_http_response::ok({{"found", found}, {"jit", found ? std::string(jit) : ""}});
    });

    // POST /api/dbg/set_party - Set module party
    router.post("/api/dbg/set_party", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("base") || !body.contains("party")) { return s_http_response::bad_request("Missing 'base' and/or 'party'"); }
        auto base = bridge.eval_expression(body["base"].get<std::string>());
        auto party_str = body["party"].get<std::string>();
        MODULEPARTY party = (party_str == "system") ? mod_system : mod_user;
        DbgFunctions()->ModSetParty(base, party);
        return s_http_response::ok({{"success", true}});
    });

    // GET /api/dbg/symbol_status - Get module symbol loading status
    router.get("/api/dbg/symbol_status", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto mod_str = req.get_query("module");
        if (mod_str.empty()) { return s_http_response::bad_request("Missing 'module'"); }
        auto base = bridge.eval_expression(mod_str);
        auto status = DbgFunctions()->ModSymbolStatus(base);
        const char* status_str = "unknown";
        if (status == MODSYMLOADED) status_str = "loaded";
        else if (status == MODSYMLOADING) status_str = "loading";
        else if (status == MODSYMLOADED) status_str = "loaded";
        return s_http_response::ok({{"module", mod_str}, {"status", status_str}});
    });

    // GET /api/dbg/mem_bp_size - Get memory breakpoint size
    router.get("/api/dbg/mem_bp_size", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        auto address_str = req.get_query("address");
        if (address_str.empty()) { return s_http_response::bad_request("Missing 'address'"); }
        auto address = bridge.eval_expression(address_str);
        return s_http_response::ok({{"address", format_utils::format_address(address)}, {"size", DbgFunctions()->MemBpSize(address)}});
    });

    // POST /api/dbg/refresh_modules - Refresh module list
    router.post("/api/dbg/refresh_modules", [](const s_http_request&) -> s_http_response {
        DbgFunctions()->RefreshModuleList();
        return s_http_response::ok({{"success", true}});
    });

    // =====================================================================
    // Memory Snapshot & Diff (XR3)
    // =====================================================================
    // In-memory snapshot storage, keyed by label.
    // Lives for the lifetime of the plugin (cleared on debugger detach/stop).
    static std::unordered_map<std::string, std::vector<uint8_t>> g_memory_snapshots;

    // POST /api/memory/snapshot - Capture a memory region snapshot
    // Body: { "address": "0x...", "size": "0x...", "label": "my_snapshot" }
    router.post("/api/memory/snapshot", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("address") || !body.contains("size") || !body.contains("label")) {
            return s_http_response::bad_request("Missing 'address', 'size', and/or 'label' fields");
        }

        auto address_str = body["address"].get<std::string>();
        auto size_str = body["size"].get<std::string>();
        auto label = body["label"].get<std::string>();

        auto address = bridge.eval_expression(address_str);
        auto size = static_cast<size_t>(std::stoull(size_str, nullptr, 16));

        if (size == 0 || size > 0x10000000) { // 256MB sanity limit
            return s_http_response::bad_request("Invalid size (must be 1 byte to 256MB)");
        }

        auto mem = bridge.read_memory(address, size);
        if (!mem.has_value()) {
            return s_http_response::internal_error("Failed to read memory at " + address_str);
        }

        g_memory_snapshots[label] = std::move(mem.value());

        return s_http_response::ok({
            {"label",   label},
            {"address", format_utils::format_address(address)},
            {"size",    size},
            {"stored",  true}
        });
    });

    // POST /api/memory/snapshot_list - List all stored snapshot labels
    router.get("/api/memory/snapshot_list", [](const s_http_request&) -> s_http_response {
        auto labels = nlohmann::json::array();
        for (const auto& [label, data] : g_memory_snapshots) {
            labels.push_back({
                {"label", label},
                {"size",  data.size()}
            });
        }
        return s_http_response::ok({
            {"snapshots", labels},
            {"count",     labels.size()}
        });
    });

    // POST /api/memory/snapshot_delete - Delete a stored snapshot by label
    router.post("/api/memory/snapshot_delete", [](const s_http_request& req) -> s_http_response {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("label")) {
            return s_http_response::bad_request("Missing 'label' field");
        }

        auto label = body["label"].get<std::string>();
        auto erased = g_memory_snapshots.erase(label);

        return s_http_response::ok({
            {"label",  label},
            {"erased", erased > 0}
        });
    });

    // POST /api/memory/diff - Compare a stored snapshot with current memory
    // Body: { "label": "my_snapshot", "address": "0x...", "size": "0x..." }
    // Returns list of changed regions with before/after bytes.
    router.post("/api/memory/diff", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("label") || !body.contains("address") || !body.contains("size")) {
            return s_http_response::bad_request("Missing 'label', 'address', and/or 'size' fields");
        }

        auto label = body["label"].get<std::string>();
        auto address_str = body["address"].get<std::string>();
        auto size_str = body["size"].get<std::string>();

        auto it = g_memory_snapshots.find(label);
        if (it == g_memory_snapshots.end()) {
            return s_http_response::not_found("Snapshot '" + label + "' not found");
        }

        auto address = bridge.eval_expression(address_str);
        auto size = static_cast<size_t>(std::stoull(size_str, nullptr, 16));

        if (size == 0 || size > 0x10000000) {
            return s_http_response::bad_request("Invalid size (must be 1 byte to 256MB)");
        }

        auto current = bridge.read_memory(address, size);
        if (!current.has_value()) {
            return s_http_response::internal_error("Failed to read current memory at " + address_str);
        }

        const auto& saved = it->second;
        const auto& curr = current.value();
        auto changes = nlohmann::json::array();
        size_t min_size = std::min(saved.size(), curr.size());

        size_t i = 0;
        while (i < min_size) {
            if (saved[i] != curr[i]) {
                // Start of a changed region
                auto change_start = address + i;
                std::vector<uint8_t> old_bytes, new_bytes;

                while (i < min_size && saved[i] != curr[i]) {
                    old_bytes.push_back(saved[i]);
                    new_bytes.push_back(curr[i]);
                    ++i;
                }

                auto change_end = address + i - 1;
                changes.push_back({
                    {"start",      format_utils::format_address(change_start)},
                    {"end",        format_utils::format_address(change_end)},
                    {"size",       old_bytes.size()},
                    {"old_bytes",  format_utils::format_bytes_hex(old_bytes.data(), old_bytes.size())},
                    {"new_bytes",  format_utils::format_bytes_hex(new_bytes.data(), new_bytes.size())}
                });
            } else {
                ++i;
            }
        }

        // Handle case where current memory is larger than saved snapshot
        if (curr.size() > saved.size()) {
            auto extra_start = address + saved.size();
            auto extra_size = curr.size() - saved.size();
            std::vector<uint8_t> new_bytes(curr.begin() + saved.size(), curr.end());
            changes.push_back({
                {"start",      format_utils::format_address(extra_start)},
                {"end",        format_utils::format_address(address + curr.size() - 1)},
                {"size",       extra_size},
                {"old_bytes",  "(beyond snapshot range)"},
                {"new_bytes",  format_utils::format_bytes_hex(new_bytes.data(), new_bytes.size())}
            });
        }

        // Compute a simple diff summary
        duint total_diff_bytes = 0;
        for (const auto& c : changes) {
            total_diff_bytes += c["size"].get<duint>();
        }

        return s_http_response::ok({
            {"label",           label},
            {"address",         format_utils::format_address(address)},
            {"size",            size},
            {"snapshot_size",    saved.size()},
            {"changes",         changes},
            {"changed_regions", changes.size()},
            {"total_diff_bytes", total_diff_bytes}
        });
    });
}

} // namespace handlers