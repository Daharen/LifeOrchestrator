#include "control_plane/control_plane.hpp"
#include "control_plane/module_registry.hpp"
#include "coordination/scheduling_coordination_stub_module.hpp"
#include "core/contracts.hpp"
#include "core/memory.hpp"
#include "core/memory_service.hpp"
#include "integration/integration_configuration_repository.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_registry_accepts_first_registration() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    auto module = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();
    auto result = registry.register_module(module);
    assert_true(result.ok, "first module registration should succeed");
}

void test_registry_rejects_duplicate_module_id() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    auto first = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();
    auto second = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();

    assert_true(registry.register_module(first).ok, "initial registration should succeed");
    auto duplicate_result = registry.register_module(second);
    assert_true(!duplicate_result.ok, "duplicate module id must be rejected");
}

void test_registry_rejects_duplicate_capability() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    auto first = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();
    auto second = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();

    assert_true(registry.register_module(first).ok, "initial registration should succeed");
    auto duplicate_result = registry.register_module(second);
    assert_true(!duplicate_result.ok, "duplicate capability ownership must be rejected");
}

void test_control_plane_dispatch_success_and_events() {
    const std::filesystem::path log_path = "artifacts/events/tests_success.ndjson";
    std::filesystem::remove(log_path);

    life_orchestrator::control_plane::ModuleRegistry registry;
    life_orchestrator::control_plane::EventLogger logger{log_path};
    auto module = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();
    assert_true(registry.register_module(module).ok, "module registration should succeed");

    life_orchestrator::control_plane::ControlPlane control_plane{registry, logger};
    life_orchestrator::core::ActionRequest request{.request_id = "req-1",
                                                   .capability_id = "scheduling.health_check",
                                                   .origin = "tests",
                                                   .requested_risk_tier =
                                                       life_orchestrator::core::RiskTier::Suggestive,
                                                   .parameters = {},
                                                   .created_at =
                                                       life_orchestrator::core::current_timestamp_utc()};
    auto response = control_plane.dispatch(request);

    assert_true(response.status == life_orchestrator::core::ExecutionStatus::Succeeded,
                "dispatch should succeed");
    assert_true(!logger.in_memory_events().empty(), "events should be recorded");
}

void test_control_plane_not_found_for_missing_capability() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/tests_not_found.ndjson"};
    life_orchestrator::control_plane::ControlPlane control_plane{registry, logger};

    life_orchestrator::core::ActionRequest request{.request_id = "req-2",
                                                   .capability_id = "missing.capability",
                                                   .origin = "tests",
                                                   .requested_risk_tier =
                                                       life_orchestrator::core::RiskTier::Informational,
                                                   .parameters = {},
                                                   .created_at =
                                                       life_orchestrator::core::current_timestamp_utc()};

    auto response = control_plane.dispatch(request);
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::NotFound,
                "missing capability should return NotFound");
}

void test_risk_rule_rejects_lower_requested_tier() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/tests_risk.ndjson"};
    auto module = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();
    assert_true(registry.register_module(module).ok, "module registration should succeed");

    life_orchestrator::control_plane::ControlPlane control_plane{registry, logger};
    life_orchestrator::core::ActionRequest request{.request_id = "req-3",
                                                   .capability_id = "scheduling.health_check",
                                                   .origin = "tests",
                                                   .requested_risk_tier =
                                                       life_orchestrator::core::RiskTier::Informational,
                                                   .parameters = {},
                                                   .created_at =
                                                       life_orchestrator::core::current_timestamp_utc()};

    auto response = control_plane.dispatch(request);
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::InvalidRequest,
                "lower requested risk should be rejected");
}

void test_file_memory_persists_and_queries() {
    namespace core = life_orchestrator::core;
    const std::filesystem::path root = "artifacts/memory/test_store";
    std::filesystem::remove_all(root);
    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/memory_store.ndjson"};

    core::FileMemoryStore store{root, &logger};
    assert_true(store.load_from_disk().ok, "load on empty store should succeed");

    core::LifeEntity entity{.entity_id = "entity.goal.1",
                            .entity_type = core::EntityType::Goal,
                            .display_name = "Stay healthy",
                            .canonical_name = "stay_healthy",
                            .description = "daily routine",
                            .created_at = core::current_timestamp_utc(),
                            .updated_at = core::current_timestamp_utc(),
                            .source_module_id = "tests.memory",
                            .version = 1,
                            .archived = false,
                            .attributes = {{"priority", "high"}}};
    assert_true(store.upsert_life_entity(entity).ok, "entity upsert should succeed");

    entity.display_name = "Stay very healthy";
    entity.version = 2;
    entity.updated_at = core::current_timestamp_utc();
    assert_true(store.upsert_life_entity(entity).ok, "entity upsert should deterministically replace materialized state");

    core::LifeRelationship rel{.relationship_id = "rel-1",
                               .from_entity_id = "entity.goal.1",
                               .to_entity_id = "entity.project.1",
                               .relationship_type = core::RelationshipType::Supports,
                               .created_at = core::current_timestamp_utc(),
                               .updated_at = core::current_timestamp_utc(),
                               .source_module_id = "tests.memory",
                               .version = 1,
                               .attributes = {{"weight", "0.8"}}};
    assert_true(store.upsert_life_relationship(rel).ok, "relationship upsert should succeed");

    assert_true(store.append_episodic_record(core::EpisodicMemoryRecord{.record_id = "ep-1",
                                                                         .timestamp = "2024-01-01T10:00:00.000Z",
                                                                         .event_type = "health_check",
                                                                         .source_module_id = "tests.memory",
                                                                         .associated_entity_ids = {"entity.goal.1"},
                                                                         .summary = "Morning routine done",
                                                                         .details = {},
                                                                         .version = 1})
                    .ok,
                "episodic append should succeed");
    assert_true(store.append_episodic_record(core::EpisodicMemoryRecord{.record_id = "ep-2",
                                                                         .timestamp = "2024-01-02T10:00:00.000Z",
                                                                         .event_type = "health_check",
                                                                         .source_module_id = "tests.memory",
                                                                         .associated_entity_ids = {"entity.goal.1"},
                                                                         .summary = "Second day",
                                                                         .details = {},
                                                                         .version = 1})
                    .ok,
                "episodic append should succeed");

    assert_true(store.upsert_preference_record(core::PreferenceRecord{.record_id = "pref-1",
                                                                       .preference_key = "meal.breakfast",
                                                                       .value = "oatmeal",
                                                                       .confidence = 0.9,
                                                                       .source_module_id = "tests.memory",
                                                                       .created_at = core::current_timestamp_utc(),
                                                                       .updated_at = core::current_timestamp_utc(),
                                                                       .version = 1})
                    .ok,
                "preference upsert should succeed");

    assert_true(store.upsert_project_memory_record(core::ProjectMemoryRecord{.record_id = "proj-rec-1",
                                                                              .project_entity_id = "entity.project.1",
                                                                              .objectives = {"ship baseline"},
                                                                              .milestones = {"sprint2"},
                                                                              .active_task_ids = {"task-1"},
                                                                              .dependency_ids = {"entity.goal.1"},
                                                                              .progress_summary = "on track",
                                                                              .source_module_id = "tests.memory",
                                                                              .created_at = core::current_timestamp_utc(),
                                                                              .updated_at = core::current_timestamp_utc(),
                                                                              .version = 1})
                    .ok,
                "project memory upsert should succeed");

    assert_true(store.append_behavioral_history_record(core::BehavioralHistoryRecord{.record_id = "beh-1",
                                                                                       .subject_key = "habit.walk",
                                                                                       .record_type = "habit",
                                                                                       .completion_state = "done",
                                                                                       .response_state = "positive",
                                                                                       .score_or_value = "1",
                                                                                       .source_module_id = "tests.memory",
                                                                                       .timestamp = "2024-01-01T06:00:00.000Z",
                                                                                       .version = 1})
                    .ok,
                "behavioral append should succeed");

    assert_true(store.persist_to_disk().ok, "persist manifest should succeed");

    core::FileMemoryStore reloaded{root, &logger};
    assert_true(reloaded.load_from_disk().ok, "load should succeed");

    auto loaded_entity = reloaded.get_entity_by_id("entity.goal.1");
    assert_true(loaded_entity.ok && loaded_entity.value->version == 2,
                "latest upserted version should materialize deterministically");

    auto relationships = reloaded.get_relationships_for_entity("entity.goal.1");
    assert_true(relationships.ok && relationships.value->size() == 1,
                "relationship query should return linked record");

    auto episodic = reloaded.list_recent_episodic_records(2);
    assert_true(episodic.ok && episodic.value->front().record_id == "ep-2",
                "episodic recency order should be deterministic");

    auto prefs = reloaded.get_preferences_by_prefix_or_key("meal.breakfast", true);
    assert_true(prefs.ok && prefs.value->size() == 1, "preference lookup should be deterministic");

    auto project = reloaded.get_project_record_by_project_entity_id("entity.project.1");
    assert_true(project.ok && project.value->record_id == "proj-rec-1",
                "project lookup by entity id should be deterministic");

    auto behavior = reloaded.list_behavioral_history_for_subject("habit.walk");
    assert_true(behavior.ok && behavior.value->size() == 1,
                "behavioral history subject lookup should be deterministic");

    bool has_load_event = false;
    bool has_write_event = false;
    for (const auto& event : logger.in_memory_events()) {
        if (event.category == core::EventCategory::MemoryLoadCompleted) {
            has_load_event = true;
        }
        if (event.category == core::EventCategory::MemoryWriteCompleted) {
            has_write_event = true;
        }
    }
    assert_true(has_load_event, "memory load should emit expected event");
    assert_true(has_write_event, "memory write should emit expected event");
}

void test_integration_configuration_repository_headless_roundtrip() {
    namespace core = life_orchestrator::core;
    const std::filesystem::path root = "artifacts/memory/integration_repo";
    std::filesystem::remove_all(root);

    life_orchestrator::integration::IntegrationConfigurationRepository repo{root};
    core::IntegrationConfigurationRecord record{.integration_config_id = "cfg-1",
                                                .integration_id = "calendar.google",
                                                .display_name = "Google Calendar",
                                                .enabled = true,
                                                .status = core::IntegrationStatus::Enabled,
                                                .capability_visibility = {"calendar.read"},
                                                .connection_diagnostics = {},
                                                .credential_storage_mode =
                                                    core::CredentialStorageMode::ExternalSecretReference,
                                                .credential_reference = "secret://calendar/google",
                                                .non_secret_settings = {},
                                                .created_at = core::current_timestamp_utc(),
                                                .updated_at = core::current_timestamp_utc(),
                                                .version = 1};
    assert_true(repo.upsert(record).ok, "integration config write should succeed");
    assert_true(repo.persist_manifest().ok, "integration config manifest should persist");

    life_orchestrator::integration::IntegrationConfigurationRepository reloaded{root};
    assert_true(reloaded.load().ok, "integration config load should succeed");

    auto loaded = reloaded.get_by_id("cfg-1");
    assert_true(loaded.has_value() && loaded->integration_id == "calendar.google",
                "integration config should reload without GUI dependency");
}

void test_corrupt_memory_record_fails_clearly() {
    const std::filesystem::path root = "artifacts/memory/corrupt_store";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "memory/life_graph");
    {
        std::ofstream out(root / "memory/life_graph/entities.ndjson");
        out << "this-is-not-valid" << '\n';
    }

    life_orchestrator::core::FileMemoryStore store{root};
    auto result = store.load_from_disk();
    assert_true(!result.ok, "corrupt records should fail clearly");
}

void test_module_can_write_memory_via_service() {
    namespace core = life_orchestrator::core;
    const std::filesystem::path root = "artifacts/memory/service_path";
    std::filesystem::remove_all(root);

    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/service_path.ndjson"};
    core::FileMemoryStore store{root, &logger};
    store.load_from_disk();
    core::MemoryService memory_service{store};

    life_orchestrator::control_plane::ModuleRegistry registry;
    auto module = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>(&memory_service);
    assert_true(registry.register_module(module).ok, "module registration should succeed");

    life_orchestrator::control_plane::ControlPlane control_plane{registry, logger};
    core::ActionRequest request{.request_id = "req-memory-service",
                                .capability_id = "scheduling.health_check",
                                .origin = "tests",
                                .requested_risk_tier = core::RiskTier::Suggestive,
                                .parameters = {},
                                .created_at = core::current_timestamp_utc()};

    auto response = control_plane.dispatch(request);
    assert_true(response.status == core::ExecutionStatus::Succeeded, "dispatch should succeed");
    auto episodes = store.list_recent_episodic_records(10);
    assert_true(episodes.ok && !episodes.value->empty(), "module execution should append episodic memory");
}

}  // namespace

int main() {
    try {
        test_registry_accepts_first_registration();
        test_registry_rejects_duplicate_module_id();
        test_registry_rejects_duplicate_capability();
        test_control_plane_dispatch_success_and_events();
        test_control_plane_not_found_for_missing_capability();
        test_risk_rule_rejects_lower_requested_tier();
        test_file_memory_persists_and_queries();
        test_integration_configuration_repository_headless_roundtrip();
        test_corrupt_memory_record_fails_clearly();
        test_module_can_write_memory_via_service();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
