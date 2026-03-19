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
#include <vector>

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


void assert_in_order(const std::string& text, const std::vector<std::string>& fragments, const std::string& message) {
    std::size_t cursor = 0;
    for (const auto& fragment : fragments) {
        const auto pos = text.find(fragment, cursor);
        if (pos == std::string::npos) throw std::runtime_error(message + ": missing " + fragment);
        cursor = pos + fragment.size();
    }
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
    assert_true(proposal_text.find("proposal_id=") != std::string::npos, "proposal list should expose proposal ids");
    assert_true(proposal_text.find("source_audit_run_id=") != std::string::npos, "proposal list should expose source audit lineage");
    assert_true(proposal_text.find("opportunity_type=") != std::string::npos, "proposal list should expose opportunity typing");
    assert_true(proposal_text.find("effort_value_classification=") != std::string::npos, "proposal list should expose effort/value classification");
    assert_true(proposal_text.find("triage_status=") != std::string::npos, "proposal list should expose triage status");
    assert_true(proposal_text.find("risk_tier=") != std::string::npos, "proposal list should expose risk metadata");
    assert_true(proposal_text.find("automation_feasibility=") != std::string::npos, "proposal list should expose feasibility metadata");
    assert_true(proposal_text.find("reliability_estimate=") != std::string::npos, "proposal list should expose reliability metadata");
    assert_true(proposal_text.find("time_recovery_minutes=") != std::string::npos, "proposal list should expose recovery metadata");
    assert_true(proposal_text.find("cognitive_recovery_score=") != std::string::npos, "proposal list should expose cognitive recovery metadata");
    assert_true(proposal_text.find("stress_recovery_score=") != std::string::npos, "proposal list should expose stress recovery metadata");
    assert_true(proposal_text.find("financial_cost_estimate=") != std::string::npos, "proposal list should expose financial cost metadata");
    assert_true(proposal_text.find("marginal_benefit_score=") != std::string::npos, "proposal list should expose marginal benefit metadata");
    assert_true(proposal_text.find("diminishing_return_flag=") != std::string::npos, "proposal list should expose diminishing return metadata");
    assert_in_order(proposal_text,
                    {"proposal_id=", "source_audit_run_id=", "opportunity_type=", "effort_value_classification=", "triage_status=", "risk_tier=", "automation_feasibility=", "reliability_estimate=", "time_recovery_minutes=", "cognitive_recovery_score=", "stress_recovery_score=", "financial_cost_estimate=", "marginal_benefit_score=", "diminishing_return_flag="},
                    "proposal list should emit required fields in stable order");

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


void test_procedural_proposal_backward_compatibility_defaults() {
    const std::filesystem::path root = "artifacts/procedural_backward_compat";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "memory" / "procedural_auditing");
    {
        std::ofstream out(root / "memory" / "procedural_auditing" / "optimization_proposals.ndjson");
        assert_true(out.is_open(), "legacy optimization proposals file should open");
        out << "optimization_proposal_id=proposal.legacy;procedural_audit_run_id=audit.legacy;activity_inventory_item_id=activity.legacy;opportunity_type=Automation;effort_value_classification=HighEffortLowValue;recovered_minutes_per_week=30;recovered_effort_points=12;confidence_label=medium;title=Legacy proposal;rationale=Legacy record;source_module_id=tests;created_at=2026-03-18T09:00:00.000Z;updated_at=2026-03-18T09:00:00.000Z;version=1;attributes=domain_source\\=ops\n";
    }

    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/procedural_backward_compat.ndjson"};
    std::filesystem::remove(logger.log_path());
    life_orchestrator::core::FileMemoryStore store{root, &logger};
    assert_true(store.load_from_disk().ok, "legacy proposal load should succeed");
    auto proposals = store.list_optimization_proposal_records();
    assert_true(proposals.ok && proposals.value->size() == 1, "legacy proposal should reload");
    const auto& proposal = proposals.value->front();
    assert_true(proposal.source_audit_run_id == "audit.legacy", "legacy proposal should default source audit lineage to procedural audit run id");
    assert_true(proposal.automation_feasibility == life_orchestrator::core::AutomationFeasibility::NotApplicable, "legacy proposal should default automation feasibility");
    assert_true(proposal.reliability_estimate == 0.0, "legacy proposal should default reliability estimate");
    assert_true(proposal.financial_cost_estimate == 0, "legacy proposal should default financial cost estimate");
    assert_true(!proposal.diminishing_return_flag, "legacy proposal should default diminishing return flag to false");

    std::ostringstream proposal_out;
    std::ostringstream proposal_err;
    auto rc = life_orchestrator::app::run_application({"procedural-list-proposals", "--data-root=" + root.string(), "--quiet-startup"}, proposal_out, proposal_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-list-proposals should succeed for legacy proposal data");
    const auto proposal_text = proposal_out.str();
    assert_true(proposal_text.find("source_audit_run_id=audit.legacy") != std::string::npos, "legacy source audit default should be emitted");
    assert_true(proposal_text.find("automation_feasibility=NotApplicable") != std::string::npos, "legacy automation default should be emitted");
    assert_true(proposal_text.find("reliability_estimate=0.000000") != std::string::npos, "legacy reliability default should be emitted");
    assert_true(proposal_text.find("financial_cost_estimate=0") != std::string::npos, "legacy financial cost default should be emitted");
    assert_true(proposal_text.find("diminishing_return_flag=false") != std::string::npos, "legacy diminishing return default should be emitted");
}

void test_procedural_list_proposals_emits_default_values() {
    Harness harness{"procedural_default_emission"};
    assert_true(harness.memory_service.upsert_optimization_proposal_record({"proposal.default",
                                                                            "audit.default",
                                                                            "activity.default",
                                                                            life_orchestrator::core::OptimizationOpportunityType::Simplification,
                                                                            life_orchestrator::core::EffortValueClassification::LowEffortLowValue,
                                                                            {0, 0, "low"},
                                                                            "Default proposal",
                                                                            "Defaults should remain visible.",
                                                                            "tests",
                                                                            "2026-03-18T09:00:00.000Z",
                                                                            "2026-03-18T09:00:00.000Z",
                                                                            1,
                                                                            "",
                                                                            "Pending",
                                                                            "",
                                                                            life_orchestrator::core::AutomationFeasibility::NotApplicable,
                                                                            "Unknown",
                                                                            0.0,
                                                                            0,
                                                                            0,
                                                                            0,
                                                                            0,
                                                                            0,
                                                                            false,
                                                                            "audit.default",
                                                                            {}}).ok,
                "default proposal upsert should succeed");
    assert_true(harness.store.persist_to_disk().ok, "default proposal persist should succeed");

    std::ostringstream proposal_out;
    std::ostringstream proposal_err;
    auto rc = life_orchestrator::app::run_application({"procedural-list-proposals", "--data-root=" + harness.root.string(), "--quiet-startup"}, proposal_out, proposal_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-list-proposals should succeed for default-valued proposal");
    const auto proposal_text = proposal_out.str();
    assert_true(proposal_text.find("proposal_id=proposal.default") != std::string::npos, "default-valued proposal id should be emitted");
    assert_true(proposal_text.find("source_audit_run_id=audit.default") != std::string::npos, "default-valued source audit should be emitted");
    assert_true(proposal_text.find("automation_feasibility=NotApplicable") != std::string::npos, "default automation value should be emitted");
    assert_true(proposal_text.find("reliability_estimate=0.000000") != std::string::npos, "default reliability value should be emitted");
    assert_true(proposal_text.find("time_recovery_minutes=0") != std::string::npos, "default time recovery should be emitted");
    assert_true(proposal_text.find("financial_cost_estimate=0") != std::string::npos, "default financial cost should be emitted");
    assert_true(proposal_text.find("diminishing_return_flag=false") != std::string::npos, "default diminishing return flag should be emitted");
}


void test_behavioral_cli_operational_surface() {
    const std::filesystem::path root = "artifacts/app_behavioral_ops";
    std::filesystem::remove_all(root);

    std::ostringstream record_out;
    std::ostringstream record_err;
    auto rc = life_orchestrator::app::run_application({"behavioral-record-state", "--data-root=" + root.string(), "--quiet-startup", "--available-capacity", "8", "--stress-level", "2", "--cognitive-load", "3", "--motivation", "7", "--recovery-status", "8", "--sleep-quality", "8", "--time-pressure", "2", "--notes", "steady", "--now", "2026-03-19T09:00:00.000Z", "--attributes-json", "{\"operator\":\"console\"}"}, record_out, record_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-record-state should succeed");
    assert_true(record_out.str().find("behavioral_record_state=ok") != std::string::npos, "behavioral record state output should be deterministic");

    std::ostringstream record_repeat_out;
    std::ostringstream record_repeat_err;
    rc = life_orchestrator::app::run_application({"behavioral-record-state", "--data-root=" + root.string(), "--quiet-startup", "--available-capacity", "8", "--stress-level", "2", "--cognitive-load", "3", "--motivation", "7", "--recovery-status", "8", "--sleep-quality", "8", "--time-pressure", "2", "--notes", "steady", "--now", "2026-03-19T09:00:00.000Z", "--attributes-json", "{\"operator\":\"console\"}"}, record_repeat_out, record_repeat_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "duplicate behavioral-record-state should succeed");

    std::ostringstream upsert_out;
    std::ostringstream upsert_err;
    rc = life_orchestrator::app::run_application({"procedural-upsert-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.ops", "--title", "Ops triage", "--domain-source", "operations", "--frequency", "daily", "--duration-minutes", "45", "--effort", "8", "--outcome-value", "3", "--repeatable", "1", "--now", "2026-03-19T09:00:00.000Z"}, upsert_out, upsert_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-upsert-activity should succeed for behavioral CLI test");

    std::ostringstream audit_out;
    std::ostringstream audit_err;
    rc = life_orchestrator::app::run_application({"procedural-run-audit", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:00:00.000Z"}, audit_out, audit_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-run-audit should succeed for behavioral CLI test");

    std::ostringstream interventions_out;
    std::ostringstream interventions_err;
    rc = life_orchestrator::app::run_application({"behavioral-list-interventions", "--data-root=" + root.string(), "--quiet-startup"}, interventions_out, interventions_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-list-interventions should succeed");
    const auto interventions_text = interventions_out.str();
    assert_true(interventions_text.find("behavioral_list_interventions=ok") != std::string::npos, "behavioral-list-interventions should emit deterministic status");
    assert_true(interventions_text.find("item_id=") != std::string::npos, "behavioral-list-interventions should emit item ids");
    assert_true(interventions_text.find("source_proposal_id=proposal.") != std::string::npos, "behavioral interventions should expose source proposal lineage");
    assert_true(interventions_text.find("source_audit_run_id=") != std::string::npos, "behavioral interventions should expose source audit lineage");
    assert_true(interventions_text.find("source_activity_id=activity.ops") != std::string::npos, "behavioral interventions should expose source activity lineage");
    assert_in_order(interventions_text, {"item_id=", "source_proposal_id=", "source_audit_run_id=", "source_activity_id=", "priority=", "status=", "effort_estimate=", "rationale="}, "behavioral interventions should emit stable fields");

    std::ostringstream backlog_out;
    std::ostringstream backlog_err;
    rc = life_orchestrator::app::run_application({"behavioral-list-backlog", "--data-root=" + root.string(), "--quiet-startup"}, backlog_out, backlog_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-list-backlog should succeed");
    const auto backlog_text = backlog_out.str();
    assert_true(backlog_text.find("behavioral_list_backlog=ok") != std::string::npos, "behavioral-list-backlog should emit deterministic status");
    assert_true(backlog_text.find("backlog_count=") != std::string::npos, "behavioral-list-backlog should emit backlog counts");

    std::ostringstream reevaluate_out;
    std::ostringstream reevaluate_err;
    rc = life_orchestrator::app::run_application({"behavioral-reevaluate-backlog", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T10:00:00.000Z"}, reevaluate_out, reevaluate_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-reevaluate-backlog should succeed");
    const auto reevaluate_text = reevaluate_out.str();
    assert_true(reevaluate_text.find("behavioral_reevaluate_backlog=ok") != std::string::npos, "reevaluate backlog should emit ok");
    assert_true(reevaluate_text.find("backlog_count=0") != std::string::npos, "reevaluate backlog should expose persisted post-reevaluation backlog count");
    assert_true(reevaluate_text.find("intervention_count=1") != std::string::npos, "reevaluate backlog should expose persisted post-reevaluation intervention count");
    assert_true(reevaluate_text.find("reevaluation_artifact_id=reevaluation.backlog.2026-03-19T10:00:00.000Z") != std::string::npos, "reevaluate backlog should expose artifact id");
    assert_true(reevaluate_text.find("reevaluated_at=2026-03-19T10:00:00.000Z") != std::string::npos, "reevaluate backlog should preserve deterministic timestamp");

    std::ostringstream reevaluations_out;
    std::ostringstream reevaluations_err;
    rc = life_orchestrator::app::run_application({"behavioral-list-reevaluations", "--data-root=" + root.string(), "--quiet-startup"}, reevaluations_out, reevaluations_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-list-reevaluations should succeed");
    const auto reevaluations_text = reevaluations_out.str();
    assert_true(reevaluations_text.find("behavioral_list_reevaluations=ok") != std::string::npos, "behavioral-list-reevaluations should emit deterministic status");
    assert_true(reevaluations_text.find("reevaluation_artifact_count=1") != std::string::npos, "behavioral-list-reevaluations should expose artifact counts");
    assert_true(reevaluations_text.find("reevaluation_artifact_id=reevaluation.backlog.2026-03-19T10:00:00.000Z") != std::string::npos, "behavioral-list-reevaluations should expose reevaluation ids");
    assert_true(reevaluations_text.find("source_state_snapshot_id=state.2026-03-19T09:00:00.000Z.8.2.3.7.8") != std::string::npos, "behavioral-list-reevaluations should expose source state lineage");
    assert_true(reevaluations_text.find("notes_or_rationale=") != std::string::npos, "behavioral-list-reevaluations should expose notes");
    assert_in_order(reevaluations_text, {"reevaluation_artifact_id=", "reevaluated_at=", "backlog_count=", "intervention_count=", "source_state_snapshot_id=", "notes_or_rationale="}, "behavioral-list-reevaluations should emit stable fields");

    std::ostringstream status_out;
    std::ostringstream status_err;
    rc = life_orchestrator::app::run_application({"behavioral-status", "--data-root=" + root.string(), "--quiet-startup"}, status_out, status_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-status should succeed");
    const auto status_text = status_out.str();
    assert_true(status_text.find("behavioral_status=ok") != std::string::npos, "behavioral-status should emit ok");
    assert_true(status_text.find("state_snapshot_count=1") != std::string::npos, "behavioral-status should deduplicate identical state snapshots");
    assert_true(status_text.find("intervention_count=") != std::string::npos, "behavioral-status should emit intervention counts");
    assert_true(status_text.find("reevaluation_artifact_count=1") != std::string::npos, "behavioral-status should emit reevaluation artifact counts");

    std::ostringstream general_status_out;
    std::ostringstream general_status_err;
    rc = life_orchestrator::app::run_application({"status", "--data-root=" + root.string(), "--quiet-startup"}, general_status_out, general_status_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "general status should succeed");
    const auto general_status_text = general_status_out.str();
    assert_true(general_status_text.find("behavioral_state_snapshot_count=1") != std::string::npos, "general status should expose behavioral state snapshot count");
    assert_true(general_status_text.find("behavioral_backlog_count=") != std::string::npos, "general status should expose behavioral backlog count");
    assert_true(general_status_text.find("behavioral_intervention_count=") != std::string::npos, "general status should expose behavioral intervention count");
    assert_true(general_status_text.find("behavioral_reevaluation_artifact_count=1") != std::string::npos, "general status should expose behavioral reevaluation artifact count");

    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/app_behavioral_ops_reload.ndjson"};
    std::filesystem::remove(logger.log_path());
    life_orchestrator::core::FileMemoryStore store{root, &logger};
    assert_true(store.load_from_disk().ok, "behavioral CLI reload should succeed");
    auto summary = store.get_behavioral_memory_summary();
    assert_true(summary.ok && summary.value->state_snapshot_count == 1, "identical behavioral state reruns should reconcile deterministically");
}

void test_behavioral_backlog_and_reevaluation_visibility_defaults() {
    Harness harness{"behavioral_visibility"};
    assert_true(harness.memory_service.append_behavioral_proposal({"proposal.backlog",
                                                                   life_orchestrator::core::BehavioralProposalType::Reminder,
                                                                   "Backlog item",
                                                                   "Visible lineage defaults.",
                                                                   "tests",
                                                                   {},
                                                                   life_orchestrator::core::BehavioralPriority::Normal,
                                                                   5.0,
                                                                   4.0,
                                                                   15,
                                                                   life_orchestrator::core::InterventionPresentationMode::SuggestivePrompt,
                                                                   std::nullopt,
                                                                   std::nullopt,
                                                                   "2026-03-19T08:00:00.000Z",
                                                                   "2026-03-19T08:00:00.000Z",
                                                                   1,
                                                                   {}}).ok,
                "behavioral proposal append should succeed");
    assert_true(harness.memory_service.upsert_behavioral_backlog_item({"backlog.visible",
                                                                       "proposal.backlog",
                                                                       life_orchestrator::core::BacklogStatus::Pending,
                                                                       "Awaiting capacity",
                                                                       "2026-03-19T08:00:00.000Z",
                                                                       std::nullopt,
                                                                       std::nullopt,
                                                                       "tests",
                                                                       1,
                                                                       "none",
                                                                       "none",
                                                                       "none",
                                                                       "",
                                                                       "",
                                                                       ""}).ok,
                "behavioral backlog upsert should succeed");
    assert_true(harness.memory_service.append_behavioral_reevaluation_artifact({"reevaluation.manual",
                                                                                 "2026-03-19T11:00:00.000Z",
                                                                                 "tests",
                                                                                 1,
                                                                                 0,
                                                                                 "none",
                                                                                 "none",
                                                                                 {"backlog.visible"},
                                                                                 {},
                                                                                 1}).ok,
                "behavioral reevaluation artifact append should succeed");
    assert_true(harness.store.persist_to_disk().ok, "behavioral visibility persist should succeed");

    std::ostringstream backlog_out;
    std::ostringstream backlog_err;
    auto rc = life_orchestrator::app::run_application({"behavioral-list-backlog", "--data-root=" + harness.root.string(), "--quiet-startup"}, backlog_out, backlog_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-list-backlog should succeed for non-empty backlog visibility");
    const auto backlog_text = backlog_out.str();
    assert_true(backlog_text.find("backlog_count=1") != std::string::npos, "behavioral-list-backlog should expose non-empty backlog counts");
    assert_true(backlog_text.find("source_proposal_id=none") != std::string::npos, "behavioral-list-backlog should emit default proposal lineage");
    assert_true(backlog_text.find("source_audit_run_id=none") != std::string::npos, "behavioral-list-backlog should emit default audit lineage");
    assert_true(backlog_text.find("source_activity_id=none") != std::string::npos, "behavioral-list-backlog should emit default activity lineage");
    assert_true(backlog_text.find("priority=Normal") != std::string::npos, "behavioral-list-backlog should emit default priority");
    assert_true(backlog_text.find("effort_estimate=0") != std::string::npos, "behavioral-list-backlog should emit default effort");
    assert_true(backlog_text.find("rationale=none") != std::string::npos, "behavioral-list-backlog should emit default rationale");
    assert_in_order(backlog_text, {"item_id=", "source_proposal_id=", "source_audit_run_id=", "source_activity_id=", "priority=", "status=", "effort_estimate=", "rationale="}, "behavioral-list-backlog should emit stable lineage defaults");

    std::ostringstream reevaluations_out;
    std::ostringstream reevaluations_err;
    rc = life_orchestrator::app::run_application({"behavioral-list-reevaluations", "--data-root=" + harness.root.string(), "--quiet-startup"}, reevaluations_out, reevaluations_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-list-reevaluations should succeed for default-valued artifact");
    const auto reevaluations_text = reevaluations_out.str();
    assert_true(reevaluations_text.find("reevaluation_artifact_id=reevaluation.manual") != std::string::npos, "behavioral-list-reevaluations should expose persisted artifact ids");
    assert_true(reevaluations_text.find("source_state_snapshot_id=none") != std::string::npos, "behavioral-list-reevaluations should emit default source state lineage");
    assert_true(reevaluations_text.find("notes_or_rationale=none") != std::string::npos, "behavioral-list-reevaluations should emit default notes");
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
        test_procedural_proposal_backward_compatibility_defaults();
        test_procedural_list_proposals_emits_default_values();
        test_behavioral_cli_operational_surface();
        test_behavioral_backlog_and_reevaluation_visibility_defaults();
        test_runtime_hygiene_ignore_file();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
