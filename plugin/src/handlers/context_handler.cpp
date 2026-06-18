#include "http/c_http_router.h"
#include "bridge/c_bridge_executor.h"
#include "util/format_utils.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace handlers {

void register_context_routes(c_http_router& router) {
    // GET /api/context/snapshot - Full state capture
    router.get("/api/context/snapshot", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();
        if (!bridge.require_paused()) {
            return s_http_response::conflict("Debugger must be paused");
        }

        auto cip = bridge.eval_expression("cip");
        auto regs = bridge.get_register_dump();

        nlohmann::json snapshot;
        snapshot["timestamp"] = time(nullptr);
        snapshot["cip"] = format_utils::format_address(cip);
        snapshot["state"] = bridge.get_state_string();

        if (regs.has_value()) {
            auto& r = regs.value();
            nlohmann::json reg_json;
            // REGISTERCONTEXT uses generic names cax/cbx/ccx/cdx/csi/cdi/csp/cbp/cip
            reg_json["cax"] = format_utils::format_address(r.regcontext.cax);
            reg_json["cbx"] = format_utils::format_address(r.regcontext.cbx);
            reg_json["ccx"] = format_utils::format_address(r.regcontext.ccx);
            reg_json["cdx"] = format_utils::format_address(r.regcontext.cdx);
            reg_json["csi"] = format_utils::format_address(r.regcontext.csi);
            reg_json["cdi"] = format_utils::format_address(r.regcontext.cdi);
            reg_json["cbp"] = format_utils::format_address(r.regcontext.cbp);
            reg_json["csp"] = format_utils::format_address(r.regcontext.csp);
            reg_json["cip"] = format_utils::format_address(r.regcontext.cip);
#ifdef _WIN64
            reg_json["r8"]  = format_utils::format_address(r.regcontext.r8);
            reg_json["r9"]  = format_utils::format_address(r.regcontext.r9);
            reg_json["r10"] = format_utils::format_address(r.regcontext.r10);
            reg_json["r11"] = format_utils::format_address(r.regcontext.r11);
            reg_json["r12"] = format_utils::format_address(r.regcontext.r12);
            reg_json["r13"] = format_utils::format_address(r.regcontext.r13);
            reg_json["r14"] = format_utils::format_address(r.regcontext.r14);
            reg_json["r15"] = format_utils::format_address(r.regcontext.r15);
#endif // _WIN64
            snapshot["registers"] = reg_json;
        }

        // Get current module info
        auto module_name = bridge.get_module_at(cip);
        if (!module_name.empty()) {
            auto mod_base = bridge.get_module_base(module_name);
            snapshot["module"] = {
                {"name", module_name},
                {"base", format_utils::format_address(mod_base)}
            };
        }

        // Get call stack summary
        auto stack_data = bridge.eval_expression("$result");
        snapshot["stack_summary"] = stack_data;

        return s_http_response::ok({
            {"snapshot", snapshot}
        });
    });

    // GET /api/context/basic - Quick register + state check
    router.get("/api/context/basic", [](const s_http_request&) -> s_http_response {
        auto& bridge = get_bridge();

        auto state = bridge.get_state_string();
        auto cip = bridge.eval_expression("cip");

        return s_http_response::ok({
            {"state", state},
            {"cip",   format_utils::format_address(cip)}
        });
    });

    // Snapshot comparison is handled by /api/context/snapshot_compare (POST) in extras_handler.cpp
    // which takes two snapshot JSON objects directly. Use x64dbg_snapshot_diff compare for this.
}

} // namespace handlers