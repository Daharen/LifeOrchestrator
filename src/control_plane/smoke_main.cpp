#include "control_plane/control_plane.hpp"
#include "coordination/scheduling_coordination_stub_module.hpp"

#include <iostream>
#include <memory>

int main() {
    using namespace life_orchestrator;

    control_plane::ModuleRegistry registry;
    control_plane::EventLogger event_logger{"artifacts/events/smoke.ndjson"};
    auto module = std::make_shared<coordination::SchedulingCoordinationStubModule>();
    registry.register_module(module);

    control_plane::ControlPlane control_plane{registry, event_logger};

    const core::ActionRequest request{
        .request_id = "smoke-request-1",
        .capability_id = "scheduling.health_check",
        .origin = "smoke",
        .requested_risk_tier = core::RiskTier::Suggestive,
        .parameters = {},
        .created_at = core::current_timestamp_utc(),
    };

    const auto response = control_plane.dispatch(request);
    std::cout << core::to_string(response.status) << ": " << response.message << '\n';
    return response.status == core::ExecutionStatus::Succeeded ? 0 : 1;
}
