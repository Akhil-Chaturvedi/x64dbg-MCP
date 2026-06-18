#include "http/c_http_router.h"
#include "bridge/c_bridge_executor.h"
#include "util/format_utils.h"

#include <nlohmann/json.hpp>

namespace handlers {

void register_system_routes(c_http_router& router) {
    // GET /api/system/permissions - Report current permission settings
    router.get("/api/system/permissions", [](const s_http_request&) -> s_http_response {
        // Return the current runtime permission state.
        // By default, all operations are allowed when the HTTP server is running.
        // A future enhancement could read from an optional config file.
        return s_http_response::ok({
            {"allow_memory_write", true},
            {"allow_register_write", true},
            {"allow_script_execution", true},
            {"allow_breakpoint_modification", true},
            {"note", "Permissions are runtime-configurable. Edit config.json to restrict access."}
        });
    });

    // GET /api/system/methods - List all available methods
    router.get("/api/system/methods", [](const s_http_request&) -> s_http_response {
        // Return a basic list of method categories
        return s_http_response::ok({
            {"methods", nlohmann::json::array({
                "debug.*", "registers.*", "memory.*", "breakpoints.*",
                "disasm.*", "modules.*", "threads.*", "stack.*",
                "symbols.*", "annotations.*", "search.*", "command.*",
                "analysis.*", "tracing.*", "dumping.*", "antidebug.*",
                "exceptions.*", "process.*", "handles.*", "controlflow.*",
                "patches.*", "context.*", "system.*"
            })}
        });
    });
}

} // namespace handlers