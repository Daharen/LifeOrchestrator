#include "app/application_bootstrap.hpp"
#include "control_plane/control_plane.hpp"
#include "coordination/behavioral_triage_module.hpp"
#include "coordination/scheduling_coordination_module.hpp"
#include "core/contracts.hpp"
#include "meta/procedural_auditor_engine.hpp"
#include "meta/procedural_auditor_module.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

void assert_true(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Harness {
    std::filesystem::path root;
    life_orchestrator::control_plane::EventLogger logger;
    life_orchestrator::core::FileMemoryStore store;
    life_orchestrator::core::MemoryService memory_service;
    life_orchestrator::control_plane::ModuleRegistry registry;
    std::shared_ptr<life_orchestrator::coordination::SchedulingCoordinationModule> scheduling;
    std::shared_ptr<life_orchestrator::coordination::BehavioralTriageModule> behavioral;
    life_orchestrator::control_plane::ControlPlane control_plane;
    std::shared_ptr<life_orchestrator::meta::ProceduralAuditorModule> procedural;

    explicit Harness(const std::string& name)
        : root("artifacts/memory/" + name),
          logger("artifacts/events/" + name + ".ndjson"),
          store(root, &logger),
          memory_service(store),
          scheduling(std::make_shared<life_orchestrator::coordination::SchedulingCoordinationModule>(&memory_service)),
          behavioral(std::make_shared<life_orchestrator::coordination::BehavioralTriageModule>(&memory_service)),
          control_plane(registry, logger),
          procedural(std::make_shared<life_orchestrator::meta::ProceduralAuditorModule>(&memory_service, &control_plane)) {
        std::filesystem::remove_all(root);
        std::filesystem::remove(logger.log_path());
        assert_true(store.load_from_disk().ok, "memory load should succeed");
        assert_true(registry.register_module(scheduling).ok, "scheduling module should register");
        assert_true(registry.register_module(behavioral).ok, "behavioral module should register");
        assert_true(registry.register_module(procedural).ok, "procedural module should register");
    }
};

life_orchestrator::core::ActionResponse dispatch(Harness& harness,
                                                 const std::string& request_id,
                                                 const std::string& capability_id,
                                                 life_orchestrator::core::StringMap parameters) {
    return harness.control_plane.dispatch({request_id, capability_id, "tests", life_orchestrator::core::RiskTier::Suggestive, std::move(parameters), life_orchestrator::core::current_timestamp_utc()});
}

void record_high_capacity(Harness& harness, const std::string& captured_at = "2026-03-18T10:00:00.000Z") {
    auto response = dispatch(harness, "state.high", "behavioral.record_state", {{"behavioral_state_snapshot_id", "state.high"}, {"captured_at", captured_at}, {"active_intervention_count", "0"}, {"backlog_count", "0"}, {"schedule_density_score", "0.2"}, {"recent_compliance_rate", "0.9"}, {"recent_failure_frequency", "0.1"}, {"fatigue_score", "0.2"}, {"stress_score", "0.2"}, {"decision_time", captured_at}});
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::Succeeded, "high capacity state should succeed");
}

void test_behavioral_application_commands_and_modules() {
    const std::filesystem::path root = "artifacts/app_behavioral";
    std::filesystem::remove_all(root);
    std::ostringstream modules_out;
    std::ostringstream modules_err;
    auto rc = life_orchestrator::app::run_application({"list-modules", "--data-root=" + root.string(), "--quiet-startup"}, modules_out, modules_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "list-modules should succeed");
    const auto modules_text = modules_out.str();
    assert_true(modules_text.find("coordination.scheduling") != std::string::npos, "scheduling module should be listed");
    assert_true(modules_text.find("coordination.behavioral_triage") != std::string::npos, "behavioral module should be listed");
    assert_true(modules_text.find("meta.procedural_auditor") != std::string::npos, "procedural module should be listed");

    std::ostringstream out;
    std::ostringstream err;
    rc = life_orchestrator::app::run_application({"behavioral-health-check", "--data-root=" + root.string(), "--quiet-startup"}, out, err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-health-check should succeed");
    assert_true(out.str().find("behavioral_health_check=ok") != std::string::npos, "behavioral health check output should be deterministic");

    std::ostringstream backlog_out;
    std::ostringstream backlog_err;
    rc = life_orchestrator::app::run_application({"behavioral-list-backlog", "--data-root=" + root.string(), "--quiet-startup"}, backlog_out, backlog_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-list-backlog should succeed");
    assert_true(backlog_out.str().find("behavioral_list_backlog=ok") != std::string::npos, "behavioral backlog output should be deterministic");
}

void test_activity_inventory_persistence_and_reload() {
    Harness harness{"procedural_persistence"};
    auto upsert = dispatch(harness, "activity.one", "procedural.upsert_activity", {{"activity_inventory_item_id", "activity.deep_work"}, {"title", "Deep work review"}, {"domain_source", "planning"}, {"frequency", "weekly"}, {"duration_minutes", "50"}, {"effort_estimate", "7"}, {"outcome_value", "4"}, {"now", "2026-03-18T09:00:00.000Z"}});
    assert_true(upsert.status == life_orchestrator::core::ExecutionStatus::Succeeded, "activity upsert should succeed");
    assert_true(harness.store.persist_to_disk().ok, "persist should succeed");

    life_orchestrator::core::FileMemoryStore reloaded{harness.root, &harness.logger};
    assert_true(reloaded.load_from_disk().ok, "reload should succeed");
    auto activity = reloaded.get_activity_inventory_item_by_id("activity.deep_work");
    assert_true(activity.ok && activity.value->title == "Deep work review", "activity inventory should reload");
}

void test_effort_value_classification() {
    life_orchestrator::meta::ProceduralAuditorEngine engine;
    life_orchestrator::core::ActivityInventoryItem item{"activity.email", "Email triage", "", "operations", "daily", 45, 8, 3, "tests", "2026-03-18T09:00:00.000Z", "2026-03-18T09:00:00.000Z", 1, {{"repeatable", "1"}}};
    assert_true(engine.classify(item) == life_orchestrator::core::EffortValueClassification::HighEffortLowValue, "classification should prioritize high effort low value");
}

void test_procedural_audit_generation_and_behavioral_routing() {
    Harness harness{"procedural_audit"};
    record_high_capacity(harness, "2026-03-18T09:00:00.000Z");
    dispatch(harness, "activity.one", "procedural.upsert_activity", {{"activity_inventory_item_id", "activity.email_triage"}, {"title", "Email triage"}, {"domain_source", "operations"}, {"frequency", "daily"}, {"duration_minutes", "45"}, {"effort_estimate", "8"}, {"outcome_value", "3"}, {"repeatable", "1"}, {"now", "2026-03-18T09:00:00.000Z"}});
    dispatch(harness, "activity.two", "procedural.upsert_activity", {{"activity_inventory_item_id", "activity.reporting"}, {"title", "Manual reporting"}, {"domain_source", "finance"}, {"frequency", "weekly"}, {"duration_minutes", "60"}, {"effort_estimate", "7"}, {"outcome_value", "4"}, {"now", "2026-03-18T09:00:00.000Z"}});
    auto audit = dispatch(harness, "audit.one", "procedural.audit_inventory", {{"procedural_audit_run_id", "audit.one"}, {"now", "2026-03-18T09:00:00.000Z"}});
    assert_true(audit.status == life_orchestrator::core::ExecutionStatus::Succeeded, "procedural audit should succeed");
    assert_true(audit.output_data.at("proposal_count") == "2", "procedural audit should generate two proposals");

    auto proposals = harness.memory_service.list_optimization_proposal_records();
    assert_true(proposals.ok && proposals.value->size() == 2, "optimization proposals should persist");
    assert_true(proposals.value->front().linked_behavioral_proposal_id == "behavioral.proposal.activity.email_triage", "procedural proposal should cross-link behavioral proposal id");
    assert_true(proposals.value->front().triage_status == "Approved", "procedural proposal should store triage outcome");

    auto behavioral = harness.memory_service.get_behavioral_proposal_by_id("behavioral.proposal.activity.email_triage");
    assert_true(behavioral.ok, "behavioral proposal should be routed through control plane");
}

void test_procedural_application_commands() {
    const std::filesystem::path root = "artifacts/app_procedural";
    std::filesystem::remove_all(root);

    std::ostringstream out;
    std::ostringstream err;
    auto rc = life_orchestrator::app::run_application({"procedural-health-check", "--data-root=" + root.string(), "--quiet-startup"}, out, err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-health-check should succeed");
    const auto output = out.str();
    assert_true(output.find("procedural_health_check=ok") != std::string::npos, "procedural health output should be deterministic");
    assert_true(output.find("first_proposal_id=proposal.activity.manual_reporting") != std::string::npos, "first proposal id should be stable");

    std::ostringstream list_out;
    std::ostringstream list_err;
    rc = life_orchestrator::app::run_application({"procedural-list-proposals", "--data-root=" + root.string(), "--quiet-startup"}, list_out, list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-list-proposals should succeed");
    assert_true(list_out.str().find("procedural_list_proposals=ok") != std::string::npos, "procedural list output should be deterministic");
    assert_true(list_out.str().find("proposal_id=proposal.activity.email_triage") != std::string::npos, "proposal list should read persisted proposals");
}

}  // namespace

int main() {
    try {
        test_behavioral_application_commands_and_modules();
        test_activity_inventory_persistence_and_reload();
        test_effort_value_classification();
        test_procedural_audit_generation_and_behavioral_routing();
        test_procedural_application_commands();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
