#pragma once

#include "control_plane/event_logger.hpp"
#include "control_plane/module_registry.hpp"

namespace life_orchestrator::control_plane {

class ControlPlane {
public:
    ControlPlane(ModuleRegistry& registry, EventLogger& event_logger);

    core::ActionResponse dispatch(const core::ActionRequest& request);

private:
    ModuleRegistry& registry_;
    EventLogger& event_logger_;

    void emit_event(core::EventCategory category,
                    const core::ActionRequest& request,
                    const std::string& module_id,
                    const std::string& message,
                    std::unordered_map<std::string, std::string> fields = {});
};

}  // namespace life_orchestrator::control_plane
