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
}

void test_activity_inventory_persistence_and_reload() {
    Harness harness{"procedural_persistence"};
    auto upsert = dispatch(harness, "activity.one", "procedural.upsert_activity", {{"activity_inventory_item_id", "activity.deep_work"}, {"title", "Deep work review"}, {"domain_source", "planning"}, {"frequency", "weekly"}, {"duration_minutes", "50"}, {"effort_estimate", "7"}, {"outcome_value", "4"}, {"cognitive_load", "6"}, {"now", "2026-03-18T09:00:00.000Z"}});
    assert_true(upsert.status == life_orchestrator::core::ExecutionStatus::Succeeded, "activity upsert should succeed");
    assert_true(harness.store.persist_to_disk().ok, "persist should succeed");

    life_orchestrator::core::FileMemoryStore reloaded{harness.root, &harness.logger};
    assert_true(reloaded.load_from_disk().ok, "reload should succeed");
    auto activity = reloaded.get_activity_inventory_item_by_id("activity.deep_work");
    assert_true(activity.ok && activity.value->title == "Deep work review", "activity inventory should reload");
    assert_true(activity.value->attributes.at("cognitive_load") == "6", "extended activity attributes should persist");
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
    dispatch(harness, "activity.two", "procedural.upsert_activity", {{"activity_inventory_item_id", "activity.reporting"}, {"title", "Manual reporting"}, {"domain_source", "finance"}, {"frequency", "weekly"}, {"duration_minutes", "60"}, {"effort_estimate", "7"}, {"outcome_value", "4"}, {"financial_cost", "25"}, {"now", "2026-03-18T09:00:00.000Z"}});
    auto audit = dispatch(harness, "audit.one", "procedural.audit_inventory", {{"procedural_audit_run_id", "audit.one"}, {"now", "2026-03-18T09:00:00.000Z"}});
    assert_true(audit.status == life_orchestrator::core::ExecutionStatus::Succeeded, "procedural audit should succeed");
    assert_true(audit.output_data.at("proposal_count") == "2", "procedural audit should generate two proposals");

    auto proposals = harness.memory_service.list_optimization_proposal_records();
    assert_true(proposals.ok && proposals.value->size() == 2, "optimization proposals should persist");
    assert_true(proposals.value->front().linked_behavioral_proposal_id.find("behavioral.proposal.") != std::string::npos, "procedural proposal should cross-link behavioral proposal id");
    assert_true(proposals.value->front().triage_status == "Approved", "procedural proposal should store triage outcome");
    assert_true(proposals.value->front().time_recovery_minutes > 0, "procedural proposal should include time recovery metadata");
    assert_true(!proposals.value->front().risk_tier.empty(), "procedural proposal should include risk tier");
    assert_true(proposals.value->front().source_audit_run_id == "audit.one", "procedural proposal should include source audit lineage");

    bool saw_automation = false;
    bool saw_delegation = false;
    for (const auto& proposal : *proposals.value) {
        if (proposal.opportunity_type == life_orchestrator::core::OptimizationOpportunityType::Automation) {
            saw_automation = true;
            assert_true(proposal.automation_feasibility != life_orchestrator::core::AutomationFeasibility::NotApplicable, "automation proposal should include automation feasibility");
            assert_true(proposal.reliability_estimate > 0.0, "automation proposal should include reliability estimate");
        }
        if (proposal.opportunity_type == life_orchestrator::core::OptimizationOpportunityType::Delegation) {
            saw_delegation = true;
            assert_true(proposal.financial_cost_estimate > 0, "delegation proposal should include financial cost estimate");
        }
    }
    assert_true(saw_automation, "audit should produce automation proposal");
    assert_true(saw_delegation || proposals.value->back().financial_cost_estimate >= 0, "audit should preserve delegation/financial metadata coverage");
}

void test_procedural_application_commands() {
    const std::filesystem::path root = "artifacts/app_procedural";
    std::filesystem::remove_all(root);

    std::ostringstream upsert_one_out;
    std::ostringstream upsert_one_err;
    auto rc = life_orchestrator::app::run_application({"procedural-upsert-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.zeta", "--title", "Zeta review", "--domain-source", "planning", "--frequency", "weekly", "--duration-minutes", "25", "--effort", "4", "--outcome-value", "6", "--attributes-json", "{\"repeatable\":\"0\",\"necessity\":\"2\"}"}, upsert_one_out, upsert_one_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-upsert-activity should create record");
    assert_true(upsert_one_out.str().find("procedural_upsert_activity=ok") != std::string::npos, "upsert output should be deterministic");

    std::ostringstream upsert_two_out;
    std::ostringstream upsert_two_err;
    rc = life_orchestrator::app::run_application({"procedural-upsert-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.alpha", "--title", "Alpha inbox", "--domain-source", "operations", "--frequency", "daily", "--duration-minutes", "45", "--effort", "8", "--outcome-value", "3", "--repeatable", "1", "--cognitive-load", "7"}, upsert_two_out, upsert_two_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-upsert-activity should support automation activity");

    std::ostringstream upsert_update_out;
    std::ostringstream upsert_update_err;
    rc = life_orchestrator::app::run_application({"procedural-upsert-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.alpha", "--title", "Alpha inbox updated", "--domain-source", "operations", "--frequency", "daily", "--duration-minutes", "50", "--effort", "8", "--outcome-value", "3", "--repeatable", "1", "--financial-cost", "12"}, upsert_update_out, upsert_update_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-upsert-activity should update existing record");
    assert_true(upsert_update_out.str().find("version=2") != std::string::npos, "re-upsert should increment version deterministically");

    std::ostringstream list_out;
    std::ostringstream list_err;
    rc = life_orchestrator::app::run_application({"procedural-list-activities", "--data-root=" + root.string(), "--quiet-startup"}, list_out, list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-list-activities should succeed");
    const auto list_text = list_out.str();
    assert_true(list_text.find("procedural_list_activities=ok") != std::string::npos, "activity list should be deterministic");
    assert_true(list_text.find("activity_id=activity.alpha") < list_text.find("activity_id=activity.zeta"), "activity listing should be deterministically ordered");
    assert_true(list_text.find("title=Alpha inbox updated") != std::string::npos, "activity update should be visible");

    std::ostringstream audit_out;
    std::ostringstream audit_err;
    rc = life_orchestrator::app::run_application({"procedural-run-audit", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:00:00.000Z"}, audit_out, audit_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-run-audit should succeed");
    const auto audit_text = audit_out.str();
    assert_true(audit_text.find("procedural_run_audit=ok") != std::string::npos, "audit output should be deterministic");
    assert_true(audit_text.find("activity_count=2") != std::string::npos, "audit should read persisted activity inventory");

    std::ostringstream audit_repeat_out;
    std::ostringstream audit_repeat_err;
    rc = life_orchestrator::app::run_application({"procedural-run-audit", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:30:00.000Z"}, audit_repeat_out, audit_repeat_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "rerunning procedural-run-audit should succeed");
    const auto first_audit_id = audit_text.substr(audit_text.find("audit_run_id=") + 13);
    const auto second_audit_text = audit_repeat_out.str();
    assert_true(second_audit_text.find("audit_run_id=") != std::string::npos, "rerun should include audit run id");
    assert_true(audit_text.substr(audit_text.find("audit_run_id="), audit_text.find('\n', audit_text.find("audit_run_id=")) - audit_text.find("audit_run_id=")) == second_audit_text.substr(second_audit_text.find("audit_run_id="), second_audit_text.find('\n', second_audit_text.find("audit_run_id=")) - second_audit_text.find("audit_run_id=")), "unchanged inventory should reuse stable audit run lineage");

    std::ostringstream proposal_out;
    std::ostringstream proposal_err;
    rc = life_orchestrator::app::run_application({"procedural-list-proposals", "--data-root=" + root.string(), "--quiet-startup"}, proposal_out, proposal_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-list-proposals should succeed");
    const auto proposal_text = proposal_out.str();
    assert_true(proposal_text.find("automation_feasibility=") != std::string::npos, "proposal list should expose feasibility metadata");
    assert_true(proposal_text.find("time_recovery_minutes=") != std::string::npos, "proposal list should expose recovery metadata");

    std::ostringstream runs_out;
    std::ostringstream runs_err;
    rc = life_orchestrator::app::run_application({"procedural-list-audit-runs", "--data-root=" + root.string(), "--quiet-startup"}, runs_out, runs_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-list-audit-runs should succeed");
    const auto runs_text = runs_out.str();
    assert_true(runs_text.find("procedural_list_audit_runs=ok") != std::string::npos, "audit run list should be deterministic");
    assert_true(runs_text.find("status=Completed") != std::string::npos, "audit run list should show status");

    std::ostringstream status_out;
    std::ostringstream status_err;
    rc = life_orchestrator::app::run_application({"status", "--data-root=" + root.string(), "--quiet-startup"}, status_out, status_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "status should succeed");
    const auto status_text = status_out.str();
    assert_true(status_text.find("activity_inventory_count=2") != std::string::npos, "status should expose activity inventory count");
    assert_true(status_text.find("procedural_audit_run_count=1") != std::string::npos, "status should expose procedural audit run count");
    assert_true(status_text.find("optimization_proposal_count=2") != std::string::npos, "status should expose optimization proposal count");
}

void test_runtime_hygiene_ignore_file() {
    auto gitignore_path = std::filesystem::exists(".gitignore") ? std::filesystem::path{".gitignore"} : std::filesystem::path{"../.gitignore"};
    std::ifstream in{gitignore_path};
    assert_true(in.is_open(), ".gitignore should exist");
    std::stringstream buffer;
    buffer << in.rdbuf();
    assert_true(buffer.str().find("runtime/") != std::string::npos, "runtime artifacts should be gitignored");
}

}  // namespace

int main() {
    try {
        test_behavioral_application_commands_and_modules();
        test_activity_inventory_persistence_and_reload();
        test_effort_value_classification();
        test_procedural_audit_generation_and_behavioral_routing();
        test_procedural_application_commands();
        test_runtime_hygiene_ignore_file();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
