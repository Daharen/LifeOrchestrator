#include "app/application_bootstrap.hpp"
#include "control_plane/control_plane.hpp"
#include "coordination/behavioral_triage_module.hpp"
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

struct Harness {
    std::filesystem::path root;
    life_orchestrator::control_plane::EventLogger logger;
    life_orchestrator::core::FileMemoryStore store;
    life_orchestrator::core::MemoryService memory_service;
    life_orchestrator::control_plane::ModuleRegistry registry;
    std::shared_ptr<life_orchestrator::coordination::SchedulingCoordinationModule> scheduling;
    std::shared_ptr<life_orchestrator::coordination::BehavioralTriageModule> behavioral;
    life_orchestrator::control_plane::ControlPlane control_plane;

    explicit Harness(const std::string& name)
        : root("artifacts/memory/" + name),
          logger("artifacts/events/" + name + ".ndjson"),
          store(root, &logger),
          memory_service(store),
          scheduling(std::make_shared<life_orchestrator::coordination::SchedulingCoordinationModule>(&memory_service)),
          behavioral(std::make_shared<life_orchestrator::coordination::BehavioralTriageModule>(&memory_service)),
          control_plane(registry, logger) {
        std::filesystem::remove_all(root);
        std::filesystem::remove(logger.log_path());
        assert_true(store.load_from_disk().ok, "memory load should succeed");
        assert_true(registry.register_module(scheduling).ok, "scheduling module should register");
        assert_true(registry.register_module(behavioral).ok, "behavioral module should register");
    }
};

life_orchestrator::core::ActionResponse dispatch(Harness& harness,
                                                 const std::string& request_id,
                                                 const std::string& capability_id,
                                                 life_orchestrator::core::StringMap parameters) {
    return harness.control_plane.dispatch({request_id, capability_id, "tests", life_orchestrator::core::RiskTier::Suggestive, std::move(parameters), life_orchestrator::core::current_timestamp_utc()});
}

void record_high_capacity(Harness& harness, const std::string& captured_at = "2026-03-18T10:00:00.000Z") {
    auto response = dispatch(harness, "state.high", "behavioral.record_state", {{"behavioral_state_snapshot_id", "state.high"}, {"captured_at", captured_at}, {"active_intervention_count", "0"}, {"backlog_count", "0"}, {"schedule_density_score", "0.2"}, {"recent_compliance_rate", "0.9"}, {"recent_failure_frequency", "0.1"}, {"fatigue_score", "0.2"}, {"stress_score", "0.2"}});
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::Succeeded, "high capacity state should succeed");
}

void record_low_capacity(Harness& harness, const std::string& captured_at = "2026-03-18T12:00:00.000Z") {
    auto response = dispatch(harness, "state.low", "behavioral.record_state", {{"behavioral_state_snapshot_id", "state.low"}, {"captured_at", captured_at}, {"active_intervention_count", "1"}, {"backlog_count", "5"}, {"schedule_density_score", "0.8"}, {"recent_compliance_rate", "0.4"}, {"recent_failure_frequency", "0.8"}, {"fatigue_score", "0.7"}, {"stress_score", "0.8"}});
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::Succeeded, "low capacity state should succeed");
}

void test_module_registration_and_record_state() {
    Harness harness{"behavioral_caps"};
    assert_true(harness.behavioral->supports_capability("behavioral.record_state"), "record_state capability missing");
    assert_true(harness.behavioral->supports_capability("behavioral.triage_proposals"), "triage capability missing");
    assert_true(harness.behavioral->supports_capability("behavioral.list_backlog"), "backlog capability missing");
    assert_true(harness.behavioral->supports_capability("behavioral.reevaluate_backlog"), "reevaluate capability missing");
    assert_true(harness.behavioral->supports_capability("behavioral.list_next_interventions"), "interventions capability missing");

    auto response = dispatch(harness, "state.medium", "behavioral.record_state", {{"behavioral_state_snapshot_id", "state.medium"}, {"captured_at", "2026-03-18T10:00:00.000Z"}, {"active_intervention_count", "0"}, {"backlog_count", "1"}, {"schedule_density_score", "0.5"}, {"recent_compliance_rate", "0.7"}, {"recent_failure_frequency", "0.2"}, {"fatigue_score", "0.3"}, {"stress_score", "0.4"}});
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::Succeeded, "record_state should succeed");
    assert_true(response.output_data.at("capacity_level") == "Medium", "capacity level should be deterministic medium");
    auto snapshots = harness.memory_service.list_recent_behavioral_state_snapshots(1);
    assert_true(snapshots.ok && snapshots.value->front().behavioral_state_snapshot_id == "state.medium", "snapshot should persist");
}

void test_triage_approve_defer_reject_and_ordering() {
    Harness harness{"behavioral_triage"};
    record_high_capacity(harness, "2026-03-18T10:00:00.000Z");
    auto approved = dispatch(harness, "triage.approve", "behavioral.triage_proposals", {{"proposal_count", "2"}, {"proposal_id1", "proposal.alpha"}, {"title1", "Stretch"}, {"priority1", "High"}, {"estimated_behavioral_effort1", "1"}, {"expected_benefit1", "5"}, {"earliest_presentation_time1", "2026-03-18T09:00:00.000Z"}, {"proposal_id2", "proposal.beta"}, {"title2", "Journal"}, {"priority2", "High"}, {"estimated_behavioral_effort2", "1"}, {"expected_benefit2", "5"}, {"earliest_presentation_time2", "2026-03-18T09:00:00.000Z"}});
    assert_true(approved.status == life_orchestrator::core::ExecutionStatus::Succeeded, "triage should succeed");
    assert_true(approved.output_data.at("approved_count") == "2", "high capacity should approve low effort proposals");
    auto interventions = harness.memory_service.list_behavioral_interventions("Approved", std::nullopt);
    assert_true(interventions.ok && interventions.value->size() == 2, "interventions should persist");
    assert_true(interventions.value->front().behavioral_proposal_id == "proposal.alpha", "stable ordering should prefer lexicographic id tie-break");

    record_low_capacity(harness, "2026-03-18T12:00:00.000Z");
    auto deferred = dispatch(harness, "triage.defer", "behavioral.triage_proposals", {{"proposal_count", "1"}, {"proposal_id", "proposal.heavy"}, {"title", "Inbox automation"}, {"priority", "High"}, {"estimated_behavioral_effort", "8"}, {"expected_benefit", "16"}, {"earliest_presentation_time", "2026-03-19T09:00:00.000Z"}});
    assert_true(deferred.output_data.at("deferred_count") == "1", "valuable high effort proposal should defer under low capacity");
    auto backlog = harness.memory_service.list_behavioral_backlog_items();
    assert_true(backlog.ok && !backlog.value->empty(), "deferred proposal should enter backlog");

    auto rejected = dispatch(harness, "triage.reject", "behavioral.triage_proposals", {{"proposal_count", "1"}, {"proposal_id", "proposal.expired"}, {"title", "Expired prompt"}, {"priority", "Critical"}, {"estimated_behavioral_effort", "1"}, {"expected_benefit", "10"}, {"latest_relevant_time", "2020-01-01T00:00:00.000Z"}});
    assert_true(rejected.output_data.at("rejected_count") == "1", "expired proposal should reject");
}

void test_reevaluate_backlog_reload_and_events() {
    Harness harness{"behavioral_reload"};
    record_low_capacity(harness, "2026-03-18T11:00:00.000Z");
    dispatch(harness, "triage.defer", "behavioral.triage_proposals", {{"proposal_count", "1"}, {"proposal_id", "proposal.promote"}, {"title", "Promote later"}, {"priority", "Critical"}, {"estimated_behavioral_effort", "3"}, {"expected_benefit", "9"}, {"earliest_presentation_time", "2026-03-18T10:00:00.000Z"}});
    record_high_capacity(harness, "2026-03-18T13:00:00.000Z");
    auto reevaluate = dispatch(harness, "reevaluate", "behavioral.reevaluate_backlog", {});
    assert_true(reevaluate.status == life_orchestrator::core::ExecutionStatus::Succeeded, "reevaluate should succeed");
    assert_true(reevaluate.output_data.at("promoted_count") == "1", "capacity increase should promote eligible backlog item");

    auto next = dispatch(harness, "interventions", "behavioral.list_next_interventions", {{"status", "Approved"}});
    assert_true(next.status == life_orchestrator::core::ExecutionStatus::Succeeded, "list_next_interventions should succeed");
    assert_true(next.output_data.at("intervention_count") == "1", "promoted intervention should be listed");

    assert_true(harness.store.persist_to_disk().ok, "persist should succeed");
    life_orchestrator::core::FileMemoryStore reloaded{harness.root, &harness.logger};
    assert_true(reloaded.load_from_disk().ok, "reload should succeed");
    auto summary = reloaded.get_behavioral_memory_summary();
    assert_true(summary.ok && summary.value->proposal_count >= 1 && summary.value->decision_count >= 2 && summary.value->intervention_count >= 1, "behavioral artifacts should survive reload");

    std::ifstream events(harness.logger.log_path());
    std::stringstream buffer;
    buffer << events.rdbuf();
    const auto event_text = buffer.str();
    assert_true(event_text.find("BehavioralStateRecorded") != std::string::npos, "behavioral state event should be appended");
    assert_true(event_text.find("BehavioralInterventionScheduled") != std::string::npos, "behavioral intervention event should be appended");
}

void test_application_commands() {
    const std::filesystem::path root = "artifacts/app_behavioral";
    std::filesystem::remove_all(root);
    std::ostringstream out;
    std::ostringstream err;
    auto rc = life_orchestrator::app::run_application({"behavioral-health-check", "--data-root=" + root.string(), "--quiet-startup"}, out, err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-health-check should succeed");
    assert_true(out.str().find("behavioral_health_check=ok") != std::string::npos, "behavioral health check output should be deterministic");

    std::ostringstream backlog_out;
    std::ostringstream backlog_err;
    rc = life_orchestrator::app::run_application({"behavioral-list-backlog", "--data-root=" + root.string(), "--quiet-startup"}, backlog_out, backlog_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-list-backlog should succeed");
    assert_true(backlog_out.str().find("behavioral_list_backlog=ok") != std::string::npos, "behavioral backlog output should be deterministic");
}

}  // namespace

int main() {
    try {
        test_module_registration_and_record_state();
        test_triage_approve_defer_reject_and_ordering();
        test_reevaluate_backlog_reload_and_events();
        test_application_commands();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
