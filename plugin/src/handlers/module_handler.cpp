#include "http/c_http_router.h"
#include "bridge/c_bridge_executor.h"
#include "util/format_utils.h"

#include <nlohmann/json.hpp>
#include "_dbgfunctions.h"
#include "_scriptapi_module.h"
#include "bridgelist.h"

namespace handlers {

void register_module_routes(c_http_router& router) {
    // GET /api/modules/list - List loaded modules
    router.get("/api/modules/list", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        // Get memory map and extract unique modules
        auto memmap = bridge.get_memory_map();
        if (!memmap.has_value()) {
            return s_http_response::internal_error(memmap.error());
        }

        std::unordered_map<std::string, nlohmann::json> modules;
        for (const auto& page : memmap.value()) {
            auto info = page.value("info", "");
            if (info.empty()) continue;

            // Check if this looks like a module section
            auto base_str = page["base"].get<std::string>();
            auto base = format_utils::parse_address(base_str);
            auto mod_name = bridge.get_module_at(base);

            if (mod_name.empty()) continue;

            if (modules.find(mod_name) == modules.end()) {
                auto mod_base = bridge.get_module_base(mod_name);
                auto mod_size = bridge.eval_expression("mod.size(" + mod_name + ")");
                auto mod_entry = bridge.eval_expression("mod.entry(" + mod_name + ")");

                modules[mod_name] = {
                    {"name",  mod_name},
                    {"base",  format_utils::format_address(mod_base)},
                    {"size",  mod_size},
                    {"entry", format_utils::format_address(mod_entry)}
                };
            }
        }

        auto result = nlohmann::json::array();
        for (const auto& [name, info] : modules) {
            result.push_back(info);
        }

        return s_http_response::ok({
            {"modules", result},
            {"count",   result.size()}
        });
    });

    // GET /api/modules/get?name=... - Module info
    router.get("/api/modules/get", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto name = req.get_query("name");
        if (name.empty()) {
            return s_http_response::bad_request("Missing 'name' query parameter");
        }

        auto base = bridge.get_module_base(name);
        if (base == 0) {
            return s_http_response::not_found("Module not found: " + name);
        }

        auto size = bridge.eval_expression("mod.size(" + name + ")");
        auto entry = bridge.eval_expression("mod.entry(" + name + ")");
        auto party = bridge.eval_expression("mod.party(" + name + ")");

        return s_http_response::ok({
            {"name",  name},
            {"base",  format_utils::format_address(base)},
            {"size",  size},
            {"entry", format_utils::format_address(entry)},
            {"party", static_cast<int>(party)} // 0=user, 1=system
        });
    });

    // GET /api/modules/base?name=... - Module base address
    router.get("/api/modules/base", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto name = req.get_query("name");
        if (name.empty()) {
            return s_http_response::bad_request("Missing 'name' query parameter");
        }

        auto base = bridge.get_module_base(name);
        if (base == 0) {
            return s_http_response::not_found("Module not found: " + name);
        }

        return s_http_response::ok({
            {"name", name},
            {"base", format_utils::format_address(base)}
        });
    });

    // GET /api/modules/section?address= - Get section name at address
    router.get("/api/modules/section", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto address_str = req.get_query("address");
        if (address_str.empty()) {
            return s_http_response::bad_request("Missing 'address' query parameter");
        }

        auto address = bridge.eval_expression(address_str);
        char section[MAX_SECTION_SIZE * 5] = {};
        auto found = DbgFunctions()->SectionFromAddr(address, section);

        return s_http_response::ok({
            {"address", format_utils::format_address(address)},
            {"found",   found},
            {"section", std::string(section)}
        });
    });

    // GET /api/modules/party?base= - Get module party (user/system)
    router.get("/api/modules/party", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto base_str = req.get_query("base");
        if (base_str.empty()) {
            return s_http_response::bad_request("Missing 'base' query parameter");
        }

        auto base = bridge.eval_expression(base_str);
        auto party = DbgFunctions()->ModGetParty(base);

        std::string party_str;
        switch (party) {
            case mod_user:   party_str = "user"; break;
            case mod_system: party_str = "system"; break;
            default:         party_str = "unknown"; break;
        }

        return s_http_response::ok({
            {"base",  format_utils::format_address(base)},
            {"party", party_str},
            {"party_id", static_cast<int>(party)}
        });
    });

    // GET /api/modules/main - Get main module info
    router.get("/api/modules/main", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        Script::Module::ModuleInfo info;
        if (!Script::Module::GetMainModuleInfo(&info)) {
            return s_http_response::not_found("No main module");
        }

        return s_http_response::ok({
            {"name",  std::string(info.name)},
            {"base",  format_utils::format_address(info.base)},
            {"size",  info.size},
            {"entry", format_utils::format_address(info.entry)},
            {"path",  std::string(info.path)},
            {"section_count", info.sectionCount}
        });
    });

    // GET /api/modules/imports?module= - List module imports
    router.get("/api/modules/imports", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto name = req.get_query("module");
        if (name.empty()) {
            return s_http_response::bad_request("Missing 'module' query parameter");
        }

        Script::Module::ModuleInfo modInfo;
        if (!Script::Module::InfoFromName(name.c_str(), &modInfo)) {
            return s_http_response::not_found("Module not found: " + name);
        }

        BridgeList<Script::Module::ModuleImport> imports;
        if (!Script::Module::GetImports(&modInfo, &imports)) {
            return s_http_response::ok({{"module", name}, {"imports", nlohmann::json::array()}, {"count", 0}});
        }

        auto result = nlohmann::json::array();
        for (int i = 0; i < imports.Count(); ++i) {
            const auto& imp = imports[i];
            result.push_back({
                {"name",           std::string(imp.name)},
                {"iat_rva",        format_utils::format_address(imp.iatRva)},
                {"iat_va",         format_utils::format_address(imp.iatVa)},
                {"ordinal",        imp.ordinal},
                {"undecorated",    imp.undecoratedName[0] ? std::string(imp.undecoratedName) : ""}
            });
        }

        return s_http_response::ok({
            {"module",  name},
            {"base",    format_utils::format_address(modInfo.base)},
            {"imports", result},
            {"count",   result.size()}
        });
    });

    // GET /api/modules/exports?module= - List module exports
    router.get("/api/modules/exports", [](const s_http_request& req) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_debugging()) {
            return s_http_response::conflict("No active debug session");
        }

        auto name = req.get_query("module");
        if (name.empty()) {
            return s_http_response::bad_request("Missing 'module' query parameter");
        }

        Script::Module::ModuleInfo modInfo;
        if (!Script::Module::InfoFromName(name.c_str(), &modInfo)) {
            return s_http_response::not_found("Module not found: " + name);
        }

        BridgeList<Script::Module::ModuleExport> exports;
        if (!Script::Module::GetExports(&modInfo, &exports)) {
            return s_http_response::ok({{"module", name}, {"exports", nlohmann::json::array()}, {"count", 0}});
        }

        auto result = nlohmann::json::array();
        for (int i = 0; i < exports.Count(); ++i) {
            const auto& exp = exports[i];
            result.push_back({
                {"name",            std::string(exp.name)},
                {"ordinal",         exp.ordinal},
                {"rva",             format_utils::format_address(exp.rva)},
                {"va",              format_utils::format_address(exp.va)},
                {"forwarded",       exp.forwarded},
                {"forward_name",    exp.forwarded && exp.forwardName[0] ? std::string(exp.forwardName) : ""},
                {"undecorated",     exp.undecoratedName[0] ? std::string(exp.undecoratedName) : ""}
            });
        }

        return s_http_response::ok({
            {"module",  name},
            {"base",    format_utils::format_address(modInfo.base)},
            {"exports", result},
            {"count",   result.size()}
        });
    });
}

} // namespace handlers
