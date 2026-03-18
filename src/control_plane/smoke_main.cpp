#include "control_plane/control_plane.hpp"
#include "coordination/scheduling_coordination_module.hpp"
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

    auto module = std::make_shared<coordination::SchedulingCoordinationModule>(&memory_service);
    registry.register_module(module);

    core::AvailabilityWindow window{"window.workday", "Workday", "2026-03-18T09:00:00.000Z", "2026-03-18T17:00:00.000Z", "UTC", "focus", "none", module->descriptor().module_id, core::current_timestamp_utc(), core::current_timestamp_utc(), 1};
    memory_service.upsert_availability_window(window);

    control_plane::ControlPlane control_plane{registry, event_logger};
    auto add_response = control_plane.dispatch({"smoke-request-1", "scheduling.add_commitment", "smoke", core::RiskTier::Suggestive, {{"title", "Existing meeting"}, {"related_entity_id", "entity.project.1"}, {"start_time", "2026-03-18T10:00:00.000Z"}, {"end_time", "2026-03-18T11:00:00.000Z"}, {"timezone", "UTC"}, {"priority", "Normal"}}, core::current_timestamp_utc()});
    auto propose_response = control_plane.dispatch({"smoke-request-2", "scheduling.propose_time_blocks", "smoke", core::RiskTier::Suggestive, {{"title", "Deep work"}, {"related_entity_id", "entity.project.1"}, {"estimated_duration_minutes", "60"}, {"earliest_start", "2026-03-18T09:00:00.000Z"}, {"latest_end", "2026-03-18T17:00:00.000Z"}, {"minimum_gap_minutes", "15"}}, core::current_timestamp_utc()});
    auto commit_response = control_plane.dispatch({"smoke-request-3", "scheduling.commit_proposal", "smoke", core::RiskTier::Suggestive, {{"proposal_id", propose_response.output_data["first_proposal_id"]}}, core::current_timestamp_utc()});

    memory_store.persist_to_disk();
    std::cout << core::to_string(add_response.status) << '\n'
              << core::to_string(propose_response.status) << '\n'
              << core::to_string(commit_response.status) << '\n';
    return add_response.status == core::ExecutionStatus::Succeeded &&
                   propose_response.status == core::ExecutionStatus::Succeeded &&
                   commit_response.status == core::ExecutionStatus::Succeeded
               ? 0
               : 1;
}
