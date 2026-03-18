#include "control_plane/control_plane.hpp"
#include "coordination/scheduling_coordination_module.hpp"
#include "core/contracts.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

void assert_true(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct SchedulingHarness {
    std::filesystem::path root;
    life_orchestrator::control_plane::EventLogger logger;
    life_orchestrator::core::FileMemoryStore store;
    life_orchestrator::core::MemoryService memory_service;
    life_orchestrator::control_plane::ModuleRegistry registry;
    std::shared_ptr<life_orchestrator::coordination::SchedulingCoordinationModule> module;
    life_orchestrator::control_plane::ControlPlane control_plane;

    explicit SchedulingHarness(const std::string& name)
        : root("artifacts/memory/" + name),
          logger("artifacts/events/" + name + ".ndjson"),
          store(root, &logger),
          memory_service(store),
          module(std::make_shared<life_orchestrator::coordination::SchedulingCoordinationModule>(&memory_service)),
          control_plane(registry, logger) {
        std::filesystem::remove_all(root);
        store.load_from_disk();
        registry.register_module(module);
    }
};

life_orchestrator::core::ActionResponse dispatch(SchedulingHarness& harness,
                                                 const std::string& request_id,
                                                 const std::string& capability_id,
                                                 life_orchestrator::core::StringMap parameters) {
    return harness.control_plane.dispatch({request_id,
                                           capability_id,
                                           "tests",
                                           life_orchestrator::core::RiskTier::Suggestive,
                                           std::move(parameters),
                                           life_orchestrator::core::current_timestamp_utc()});
}

void add_window(SchedulingHarness& harness,
                const std::string& id,
                const std::string& start_time,
                const std::string& end_time) {
    using namespace life_orchestrator::core;
    harness.memory_service.upsert_availability_window({id,
                                                       id,
                                                       start_time,
                                                       end_time,
                                                       "UTC",
                                                       "focus",
                                                       "none",
                                                       harness.module->descriptor().module_id,
                                                       current_timestamp_utc(),
                                                       current_timestamp_utc(),
                                                       1});
}

void test_registry_and_capabilities() {
    SchedulingHarness harness{"scheduling_caps"};
    const auto& descriptor = harness.module->descriptor();
    assert_true(descriptor.module_id == "coordination.scheduling", "module id should be authoritative scheduling module");
    assert_true(harness.module->supports_capability("scheduling.add_commitment"), "add_commitment capability missing");
    assert_true(harness.module->supports_capability("scheduling.detect_conflicts"), "detect_conflicts capability missing");
    assert_true(harness.module->supports_capability("scheduling.propose_time_blocks"), "propose_time_blocks capability missing");
    assert_true(harness.module->supports_capability("scheduling.commit_proposal"), "commit_proposal capability missing");
    assert_true(harness.module->supports_capability("scheduling.list_schedule_window"), "list_schedule_window capability missing");
}

void test_add_commitment_persists() {
    SchedulingHarness harness{"add_commitment"};
    add_window(harness, "window.1", "2026-03-18T09:00:00.000Z", "2026-03-18T17:00:00.000Z");
    auto response = dispatch(harness, "req-add", "scheduling.add_commitment", {{"title", "Dentist"}, {"related_entity_id", "entity.health.1"}, {"start_time", "2026-03-18T10:00:00.000Z"}, {"end_time", "2026-03-18T11:00:00.000Z"}, {"timezone", "UTC"}, {"priority", "High"}});
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::Succeeded, "add_commitment should succeed");
    auto commitment = harness.memory_service.get_commitment_by_id(response.output_data.at("commitment_id"));
    assert_true(commitment.ok && commitment.value->title == "Dentist", "commitment should persist in memory store");
}

void test_detect_conflicts_overlap() {
    SchedulingHarness harness{"detect_conflicts"};
    add_window(harness, "window.1", "2026-03-18T09:00:00.000Z", "2026-03-18T17:00:00.000Z");
    dispatch(harness, "req-a", "scheduling.add_commitment", {{"schedule_item_id", "commitment.a"}, {"title", "A"}, {"related_entity_id", "entity.1"}, {"start_time", "2026-03-18T10:00:00.000Z"}, {"end_time", "2026-03-18T11:00:00.000Z"}, {"timezone", "UTC"}});
    dispatch(harness, "req-b", "scheduling.add_commitment", {{"schedule_item_id", "commitment.b"}, {"title", "B"}, {"related_entity_id", "entity.2"}, {"start_time", "2026-03-18T10:30:00.000Z"}, {"end_time", "2026-03-18T11:30:00.000Z"}, {"timezone", "UTC"}});
    auto response = dispatch(harness, "req-conflicts", "scheduling.detect_conflicts", {{"start_time", "2026-03-18T09:00:00.000Z"}, {"end_time", "2026-03-18T12:00:00.000Z"}});
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::Succeeded, "detect_conflicts should succeed");
    auto conflicts = harness.memory_service.list_conflicts("2026-03-18T00:00:00.000Z", "2026-03-18T23:59:59.000Z", std::nullopt);
    assert_true(conflicts.ok && !conflicts.value->empty(), "overlap conflict should be persisted deterministically");
}

void test_stable_proposal_ordering_and_rejection() {
    SchedulingHarness harness{"proposals"};
    add_window(harness, "window.1", "2026-03-18T09:00:00.000Z", "2026-03-18T12:00:00.000Z");
    add_window(harness, "window.2", "2026-03-18T13:00:00.000Z", "2026-03-18T17:00:00.000Z");
    dispatch(harness, "req-existing", "scheduling.add_commitment", {{"schedule_item_id", "commitment.block"}, {"title", "Meeting"}, {"related_entity_id", "entity.1"}, {"start_time", "2026-03-18T10:00:00.000Z"}, {"end_time", "2026-03-18T11:00:00.000Z"}, {"timezone", "UTC"}});
    auto first = dispatch(harness, "req-propose-1", "scheduling.propose_time_blocks", {{"schedule_item_id", "task.alpha"}, {"title", "Work block"}, {"related_entity_id", "entity.1"}, {"estimated_duration_minutes", "30"}, {"earliest_start", "2026-03-18T09:00:00.000Z"}, {"latest_end", "2026-03-18T17:00:00.000Z"}, {"minimum_gap_minutes", "15"}});
    auto proposals = harness.memory_service.list_proposals_for_task_candidate("task.alpha");
    assert_true(first.status == life_orchestrator::core::ExecutionStatus::Succeeded, "proposal generation should succeed");
    assert_true(proposals.ok && proposals.value->size() >= 2, "expected multiple proposals for deterministic ordering");
    assert_true(proposals.value->front().proposal_rank == 1, "first proposal rank should be stable");

    auto rejected = dispatch(harness, "req-propose-2", "scheduling.propose_time_blocks", {{"schedule_item_id", "task.impossible"}, {"title", "Impossible"}, {"related_entity_id", "entity.1"}, {"estimated_duration_minutes", "240"}, {"earliest_start", "2026-03-18T09:00:00.000Z"}, {"latest_end", "2026-03-18T10:00:00.000Z"}, {"minimum_gap_minutes", "15"}});
    assert_true(rejected.status == life_orchestrator::core::ExecutionStatus::Rejected, "impossible windows should reject cleanly");
}

void test_commit_proposal_success_and_stale_failure() {
    SchedulingHarness harness{"commit_proposal"};
    add_window(harness, "window.1", "2026-03-18T09:00:00.000Z", "2026-03-18T17:00:00.000Z");
    auto propose = dispatch(harness, "req-propose", "scheduling.propose_time_blocks", {{"schedule_item_id", "task.commit"}, {"title", "Write report"}, {"related_entity_id", "entity.9"}, {"estimated_duration_minutes", "60"}, {"earliest_start", "2026-03-18T09:00:00.000Z"}, {"latest_end", "2026-03-18T17:00:00.000Z"}});
    assert_true(propose.status == life_orchestrator::core::ExecutionStatus::Succeeded, "proposal generation should succeed before commit");
    auto commit = dispatch(harness, "req-commit", "scheduling.commit_proposal", {{"proposal_id", propose.output_data.at("first_proposal_id")}});
    assert_true(commit.status == life_orchestrator::core::ExecutionStatus::Succeeded, "commit_proposal should succeed");
    auto resulting = harness.memory_service.get_commitment_by_id(commit.output_data.at("commitment_id"));
    assert_true(resulting.ok, "committed proposal should materialize a scheduled commitment");

    auto second = dispatch(harness, "req-propose-second", "scheduling.propose_time_blocks", {{"schedule_item_id", "task.stale"}, {"title", "Later work"}, {"related_entity_id", "entity.10"}, {"estimated_duration_minutes", "60"}, {"earliest_start", "2026-03-18T09:00:00.000Z"}, {"latest_end", "2026-03-18T17:00:00.000Z"}});
    auto stale_proposal = harness.memory_service.get_proposal_by_id(second.output_data.at("first_proposal_id"));
    assert_true(stale_proposal.ok, "stale test proposal should exist");
    dispatch(harness, "req-overlap", "scheduling.add_commitment", {{"title", "Collision"}, {"related_entity_id", "entity.11"}, {"start_time", stale_proposal.value->proposed_start_time}, {"end_time", stale_proposal.value->proposed_end_time}, {"timezone", "UTC"}});
    auto stale = dispatch(harness, "req-stale", "scheduling.commit_proposal", {{"proposal_id", second.output_data.at("first_proposal_id")}});
    assert_true(stale.status == life_orchestrator::core::ExecutionStatus::Rejected, "stale proposal should fail clearly");
}

void test_list_window_gap_availability_events_and_reload() {
    SchedulingHarness harness{"list_window"};
    add_window(harness, "window.1", "2026-03-18T09:00:00.000Z", "2026-03-18T12:00:00.000Z");
    add_window(harness, "window.2", "2026-03-18T13:00:00.000Z", "2026-03-18T17:00:00.000Z");
    dispatch(harness, "req-add-1", "scheduling.add_commitment", {{"schedule_item_id", "commitment.1"}, {"title", "Earlier"}, {"related_entity_id", "entity.1"}, {"start_time", "2026-03-18T09:30:00.000Z"}, {"end_time", "2026-03-18T10:00:00.000Z"}, {"timezone", "UTC"}});
    dispatch(harness, "req-add-2", "scheduling.add_commitment", {{"schedule_item_id", "commitment.2"}, {"title", "Later"}, {"related_entity_id", "entity.2"}, {"start_time", "2026-03-18T11:00:00.000Z"}, {"end_time", "2026-03-18T11:30:00.000Z"}, {"timezone", "UTC"}});
    auto propose = dispatch(harness, "req-gap", "scheduling.propose_time_blocks", {{"schedule_item_id", "task.gap"}, {"title", "Gap task"}, {"related_entity_id", "entity.3"}, {"estimated_duration_minutes", "30"}, {"earliest_start", "2026-03-18T09:00:00.000Z"}, {"latest_end", "2026-03-18T17:00:00.000Z"}, {"minimum_gap_minutes", "15"}});
    assert_true(propose.status == life_orchestrator::core::ExecutionStatus::Succeeded, "gap proposal should succeed");
    auto window_response = dispatch(harness, "req-list", "scheduling.list_schedule_window", {{"start_time", "2026-03-18T09:00:00.000Z"}, {"end_time", "2026-03-18T17:00:00.000Z"}});
    assert_true(window_response.status == life_orchestrator::core::ExecutionStatus::Succeeded, "list_schedule_window should succeed");
    assert_true(window_response.output_data.at("commitment_count") == "2", "window listing should report commitments in deterministic order");

    bool saw_memory_write = false;
    for (const auto& event : harness.logger.in_memory_events()) {
        if (event.category == life_orchestrator::core::EventCategory::MemoryWriteCompleted) saw_memory_write = true;
    }
    assert_true(saw_memory_write, "scheduling operations should emit observable memory write events");

    assert_true(harness.store.persist_to_disk().ok, "persist should succeed");
    life_orchestrator::core::FileMemoryStore reloaded{harness.root, &harness.logger};
    assert_true(reloaded.load_from_disk().ok, "reload should succeed");
    auto summary = reloaded.get_memory_summary();
    assert_true(summary.ok && summary.value->scheduling_commitment_count >= 2 && summary.value->scheduling_proposal_count >= 1, "scheduling records should survive reload");
}

void test_existing_integration_repo_and_corrupt_memory_still_hold() {
    namespace core = life_orchestrator::core;
    const std::filesystem::path root = "artifacts/memory/integration_repo";
    std::filesystem::remove_all(root);

    life_orchestrator::integration::IntegrationConfigurationRepository repo{root};
    core::IntegrationConfigurationRecord record{"cfg-1", "calendar.google", "Google Calendar", true, core::IntegrationStatus::Enabled, {"calendar.read"}, {}, core::CredentialStorageMode::ExternalSecretReference, "secret://calendar/google", {}, core::current_timestamp_utc(), core::current_timestamp_utc(), 1};
    assert_true(repo.upsert(record).ok, "integration config write should succeed");
    assert_true(repo.persist_manifest().ok, "integration config manifest should persist");

    life_orchestrator::integration::IntegrationConfigurationRepository reloaded{root};
    assert_true(reloaded.load().ok, "integration config load should succeed");
    auto loaded = reloaded.get_by_id("cfg-1");
    assert_true(loaded.has_value() && loaded->integration_id == "calendar.google", "integration config should reload without GUI dependency");

    const std::filesystem::path corrupt_root = "artifacts/memory/corrupt_store";
    std::filesystem::remove_all(corrupt_root);
    std::filesystem::create_directories(corrupt_root / "memory/life_graph");
    std::ofstream(corrupt_root / "memory/life_graph/entities.ndjson") << "this-is-not-valid\n";
    life_orchestrator::core::FileMemoryStore corrupt_store{corrupt_root};
    auto load_result = corrupt_store.load_from_disk();
    assert_true(!load_result.ok, "corrupt records should fail clearly");
}

}  // namespace

int main() {
    try {
        test_registry_and_capabilities();
        test_add_commitment_persists();
        test_detect_conflicts_overlap();
        test_stable_proposal_ordering_and_rejection();
        test_commit_proposal_success_and_stale_failure();
        test_list_window_gap_availability_events_and_reload();
        test_existing_integration_repo_and_corrupt_memory_still_hold();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
