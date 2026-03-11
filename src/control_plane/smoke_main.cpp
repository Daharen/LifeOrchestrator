#include "control_plane/control_plane.hpp"
#include "coordination/scheduling_coordination_stub_module.hpp"
#include "core/memory.hpp"
#include "core/memory_service.hpp"

#include <iostream>
#include <memory>

int main() {
    using namespace life_orchestrator;

    control_plane::ModuleRegistry registry;
    control_plane::EventLogger event_logger{"artifacts/events/smoke.ndjson"};

    core::FileMemoryStore memory_store{"data", &event_logger};
    memory_store.load_from_disk();
    core::MemoryService memory_service{memory_store};

    auto module = std::make_shared<coordination::SchedulingCoordinationStubModule>(&memory_service);
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
    memory_store.persist_to_disk();
    std::cout << core::to_string(response.status) << ": " << response.message << '\n';
    return response.status == core::ExecutionStatus::Succeeded ? 0 : 1;
}
