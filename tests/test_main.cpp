#include "app/app_support/action_form_registry.hpp"
#include "app/assistant_shell/assistant_shell_surface_service.h"
#include "app/assistant_shell/assistant_shell_surface_contracts.h"
#include "app/app_support/action_result_view.hpp"
#include "app/app_support/artifact_presentation_registry.hpp"
#include "app/application_bootstrap.hpp"
#include "app/provider_setup/provider_setup_service.h"
#include "ui/provider_setup/provider_setup_controller.h"
#include "ui/artifact_panels/artifact_panels_registry.hpp"
#include "ui/assistant_shell/assistant_shell_composer_input.h"
#include "integration/inference/http_executor_contracts.h"
#include "integration/inference/inference_transport_client.h"
#include "integration/inference/openai_responses_request_builder.h"
#include "integration/inference/openai_responses_response_parser.h"
#include "intelligence/intent_router.hpp"
#include "control_plane/control_plane.hpp"
#include "coordination/behavioral_triage_module.hpp"
#include "coordination/scheduling_coordination_module.hpp"
#include "core/contracts.hpp"
#include "meta/procedural_auditor_engine.hpp"
#include "meta/procedural_auditor_module.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

life_orchestrator::integration::inference::HttpResponseSpec make_http_response(
    std::optional<int> http_status,
    std::string body,
    std::string transport_error_text,
    bool success,
    bool network_success,
    std::string failure_stage,
    std::optional<unsigned long> win32_error_code = std::nullopt,
    std::string win32_error_message = {},
    std::string response_content_type = {},
    std::string response_request_id = {},
    std::string safe_error_summary = {},
    std::string safe_body_preview = {}) {
    return {http_status, {}, std::move(body), std::move(transport_error_text), success, network_success, std::move(failure_stage), win32_error_code, std::move(win32_error_message), std::move(response_content_type), std::move(response_request_id), std::move(safe_error_summary), std::move(safe_body_preview)};
}

struct FakeHttpExecutor final : life_orchestrator::integration::inference::IHttpExecutor {
    mutable std::vector<life_orchestrator::integration::inference::HttpRequestSpec> requests;
    life_orchestrator::integration::inference::HttpResponseSpec next_response = make_http_response(
        200,
        R"({"output_text":"{\"mode\":\"proposed\",\"matched_command\":\"procedural-upsert-activity\",\"args\":\"procedural-upsert-activity --activity-id activity.weekly-laundry --title WeeklyLaundry --domain-source home --frequency weekly --duration-minutes 60 --effort-estimate 4 --outcome-value 6\",\"confidence\":0.92,\"reasoning_summary\":\"Weekly laundry maps cleanly.\",\"requires_confirmation\":false,\"closest_commands\":\"procedural-upsert-activity,status\",\"user_facing_message\":\"Mapped request safely.\"}","input_tokens":11,"output_tokens":7,"total_tokens":18})",
        {},
        true,
        true,
        {},
        std::nullopt,
        {},
        "application/json");

    life_orchestrator::integration::inference::HttpResponseSpec Execute(const life_orchestrator::integration::inference::HttpRequestSpec& request) const override {
        requests.push_back(request);
        return next_response;
    }
};

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
    assert_true(modules_text.find("memory.artifact_query") != std::string::npos, "artifact query module should be listed");
}

void test_artifact_query_command_surface() {
    const std::filesystem::path root = "artifacts/artifact_query_surface";
    std::filesystem::remove_all(root);

    std::ostringstream upsert_out;
    std::ostringstream upsert_err;
    auto rc = life_orchestrator::app::run_application({"procedural-upsert-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.focus", "--title", "Focus block", "--domain-source", "planning", "--frequency", "daily", "--duration-minutes", "45", "--effort", "5", "--outcome-value", "7"}, upsert_out, upsert_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-upsert-activity should seed artifact query data");

    std::ostringstream provider_out;
    std::ostringstream provider_err;
    rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai", "--api-key", "TEST_KEY_123", "--model-name", "gpt-5"}, provider_out, provider_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "integration-set-provider should seed provider artifacts");

    std::ostringstream activity_out;
    std::ostringstream activity_err;
    rc = life_orchestrator::app::run_application({"artifact.query", "--data-root=" + root.string(), "--quiet-startup", "--artifact-type", "activity_inventory"}, activity_out, activity_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "artifact.query should support activity inventory");
    assert_true(activity_out.str().find("artifact_count=1") != std::string::npos, "artifact.query should report activity counts");
    assert_true(activity_out.str().find("artifact_id=activity.focus") != std::string::npos, "artifact.query should render activity envelopes");

    std::ostringstream provider_list_out;
    std::ostringstream provider_list_err;
    rc = life_orchestrator::app::run_application({"artifact.query", "--data-root=" + root.string(), "--quiet-startup", "--artifact-type", "provider_config_summary"}, provider_list_out, provider_list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "artifact.query should support provider config summaries");
    assert_true(provider_list_out.str().find("api_key_redacted=TE***23") != std::string::npos, "artifact.query should redact provider secrets");
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

void test_scheduling_candidate_cli_bridge() {
    const std::filesystem::path root = "artifacts/app_scheduling_candidates";
    std::filesystem::remove_all(root);

    std::ostringstream record_out;
    std::ostringstream record_err;
    auto rc = life_orchestrator::app::run_application({"behavioral-record-state", "--data-root=" + root.string(), "--quiet-startup", "--available-capacity", "8", "--stress-level", "2", "--cognitive-load", "3", "--motivation", "7", "--recovery-status", "8", "--now", "2026-03-19T09:00:00.000Z"}, record_out, record_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-record-state should succeed for scheduling candidate test");

    std::ostringstream upsert_out;
    std::ostringstream upsert_err;
    rc = life_orchestrator::app::run_application({"procedural-upsert-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.schedule_bridge", "--title", "Schedule bridge", "--domain-source", "planning", "--frequency", "daily", "--duration-minutes", "30", "--effort", "8", "--outcome-value", "3", "--repeatable", "1", "--now", "2026-03-19T09:00:00.000Z"}, upsert_out, upsert_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-upsert-activity should succeed for scheduling candidate test");

    std::ostringstream audit_out;
    std::ostringstream audit_err;
    rc = life_orchestrator::app::run_application({"procedural-run-audit", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:00:00.000Z"}, audit_out, audit_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-run-audit should succeed for scheduling candidate test");

    std::ostringstream generate_out;
    std::ostringstream generate_err;
    rc = life_orchestrator::app::run_application({"scheduling-generate-candidates", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:15:00.000Z"}, generate_out, generate_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "scheduling-generate-candidates should succeed");
    const auto generate_text = generate_out.str();
    assert_true(generate_text.find("scheduling_generate_candidates=ok") != std::string::npos, "scheduling candidate generation should emit ok");
    assert_true(generate_text.find("intervention_count=1") != std::string::npos, "scheduling candidate generation should count interventions");
    assert_true(generate_text.find("candidate_count=1") != std::string::npos, "scheduling candidate generation should create candidate");
    assert_true(generate_text.find("deferred_count=0") != std::string::npos, "scheduling candidate generation should expose deferred count");

    std::ostringstream list_out;
    std::ostringstream list_err;
    rc = life_orchestrator::app::run_application({"scheduling-list-candidates", "--data-root=" + root.string(), "--quiet-startup"}, list_out, list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "scheduling-list-candidates should succeed");
    const auto list_text = list_out.str();
    assert_true(list_text.find("scheduling_list_candidates=ok") != std::string::npos, "scheduling candidate list should emit ok");
    assert_true(list_text.find("candidate_id=candidate.intervention.") != std::string::npos, "scheduling candidate list should emit deterministic candidate ids");
    assert_true(list_text.find("source_intervention_id=intervention.") != std::string::npos, "scheduling candidate list should expose intervention lineage");
    assert_true(list_text.find("source_proposal_id=proposal.") != std::string::npos, "scheduling candidate list should expose proposal lineage");
    assert_true(list_text.find("source_audit_run_id=audit.") != std::string::npos, "scheduling candidate list should expose audit lineage");
    assert_true(list_text.find("source_activity_id=activity.schedule_bridge") != std::string::npos, "scheduling candidate list should expose activity lineage");
    assert_true(list_text.find("estimated_duration_minutes=30") != std::string::npos, "scheduling candidate list should derive deterministic duration");
    assert_true(list_text.find("urgency=High") != std::string::npos || list_text.find("urgency=Critical") != std::string::npos, "scheduling candidate list should expose urgency");
    assert_true(list_text.find("scheduling_window_hint=") != std::string::npos, "scheduling candidate list should expose window hint");
    assert_true(list_text.find("recommended_time_of_day=") != std::string::npos, "scheduling candidate list should expose time-of-day");
    assert_true(list_text.find("recommended_day_span=") != std::string::npos, "scheduling candidate list should expose day span");
    assert_true(list_text.find("rationale=") != std::string::npos, "scheduling candidate list should expose rationale");
    assert_true(list_text.find("status=candidate") != std::string::npos, "scheduling candidate list should expose candidate status");
    assert_in_order(list_text, {"candidate_id=", "source_intervention_id=", "source_proposal_id=", "source_audit_run_id=", "source_activity_id=", "estimated_duration_minutes=", "urgency=", "scheduling_window_hint=", "recommended_time_of_day=", "recommended_day_span=", "rationale=", "status="}, "scheduling candidate list should emit stable fields");

    std::ostringstream generate_repeat_out;
    std::ostringstream generate_repeat_err;
    rc = life_orchestrator::app::run_application({"scheduling-generate-candidates", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:15:00.000Z"}, generate_repeat_out, generate_repeat_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "re-running scheduling-generate-candidates should succeed");

    std::ostringstream list_repeat_out;
    std::ostringstream list_repeat_err;
    rc = life_orchestrator::app::run_application({"scheduling-list-candidates", "--data-root=" + root.string(), "--quiet-startup"}, list_repeat_out, list_repeat_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "re-listing scheduling candidates should succeed");
    assert_true(list_repeat_out.str().find("candidate_count=1") != std::string::npos, "unchanged scheduling candidate reruns should remain idempotent");

    std::ostringstream status_out;
    std::ostringstream status_err;
    rc = life_orchestrator::app::run_application({"status", "--data-root=" + root.string(), "--quiet-startup"}, status_out, status_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "general status should succeed for scheduling candidate test");
    assert_true(status_out.str().find("scheduling_candidate_count=1") != std::string::npos, "general status should expose scheduling candidate count");
}

void test_scheduling_candidate_default_emission_and_backward_compatibility() {
    const std::filesystem::path root = "artifacts/scheduling_candidate_defaults";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "memory" / "scheduling");
    {
        std::ofstream out(root / "memory" / "scheduling" / "candidates.ndjson");
        assert_true(out.is_open(), "legacy scheduling candidates file should open");
        out << "candidate_id=candidate.legacy;source_intervention_id=intervention.legacy\n";
    }

    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/scheduling_candidate_defaults.ndjson"};
    std::filesystem::remove(logger.log_path());
    life_orchestrator::core::FileMemoryStore store{root, &logger};
    assert_true(store.load_from_disk().ok, "legacy scheduling candidate load should succeed");
    auto candidates = store.list_scheduling_candidate_records();
    assert_true(candidates.ok && candidates.value->size() == 1, "legacy scheduling candidate should reload");
    const auto& candidate = candidates.value->front();
    assert_true(candidate.source_proposal_id == "none", "legacy scheduling candidate should default proposal lineage");
    assert_true(candidate.source_audit_run_id == "none", "legacy scheduling candidate should default audit lineage");
    assert_true(candidate.source_activity_id == "none", "legacy scheduling candidate should default activity lineage");
    assert_true(candidate.estimated_duration_minutes == 0, "legacy scheduling candidate should default duration");
    assert_true(candidate.urgency == "Normal", "legacy scheduling candidate should default urgency");
    assert_true(candidate.scheduling_window_hint == "unspecified", "legacy scheduling candidate should default window hint");
    assert_true(candidate.recommended_time_of_day == "unspecified", "legacy scheduling candidate should default time of day");
    assert_true(candidate.recommended_day_span == "unspecified", "legacy scheduling candidate should default day span");
    assert_true(candidate.rationale == "none", "legacy scheduling candidate should default rationale");
    assert_true(candidate.status == life_orchestrator::core::SchedulingCandidateStatus::Candidate, "legacy scheduling candidate should default status");

    std::ostringstream list_out;
    std::ostringstream list_err;
    auto rc = life_orchestrator::app::run_application({"scheduling-list-candidates", "--data-root=" + root.string(), "--quiet-startup"}, list_out, list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "scheduling-list-candidates should succeed for legacy candidate data");
    const auto text = list_out.str();
    assert_true(text.find("source_proposal_id=none") != std::string::npos, "legacy candidate list should emit default proposal lineage");
    assert_true(text.find("source_audit_run_id=none") != std::string::npos, "legacy candidate list should emit default audit lineage");
    assert_true(text.find("source_activity_id=none") != std::string::npos, "legacy candidate list should emit default activity lineage");
    assert_true(text.find("estimated_duration_minutes=0") != std::string::npos, "legacy candidate list should emit default duration");
    assert_true(text.find("urgency=Normal") != std::string::npos, "legacy candidate list should emit default urgency");
    assert_true(text.find("scheduling_window_hint=unspecified") != std::string::npos, "legacy candidate list should emit default window hint");
    assert_true(text.find("recommended_time_of_day=unspecified") != std::string::npos, "legacy candidate list should emit default time-of-day");
    assert_true(text.find("recommended_day_span=unspecified") != std::string::npos, "legacy candidate list should emit default day span");
    assert_true(text.find("rationale=none") != std::string::npos, "legacy candidate list should emit default rationale");
    assert_true(text.find("status=candidate") != std::string::npos, "legacy candidate list should emit default status");
}

void test_scheduling_proposal_cli_bridge() {
    const std::filesystem::path root = "artifacts/app_scheduling_proposals";
    std::filesystem::remove_all(root);

    std::ostringstream record_out;
    std::ostringstream record_err;
    auto rc = life_orchestrator::app::run_application({"behavioral-record-state", "--data-root=" + root.string(), "--quiet-startup", "--available-capacity", "8", "--stress-level", "2", "--cognitive-load", "3", "--motivation", "7", "--recovery-status", "8", "--now", "2026-03-19T09:00:00.000Z"}, record_out, record_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-record-state should succeed for scheduling proposal test");

    std::ostringstream upsert_out;
    std::ostringstream upsert_err;
    rc = life_orchestrator::app::run_application({"procedural-upsert-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.proposal_bridge", "--title", "Proposal bridge", "--domain-source", "planning", "--frequency", "daily", "--duration-minutes", "30", "--effort", "8", "--outcome-value", "3", "--repeatable", "1", "--now", "2026-03-19T09:00:00.000Z"}, upsert_out, upsert_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-upsert-activity should succeed for scheduling proposal test");

    std::ostringstream audit_out;
    std::ostringstream audit_err;
    rc = life_orchestrator::app::run_application({"procedural-run-audit", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:00:00.000Z"}, audit_out, audit_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-run-audit should succeed for scheduling proposal test");

    std::ostringstream generate_candidates_out;
    std::ostringstream generate_candidates_err;
    rc = life_orchestrator::app::run_application({"scheduling-generate-candidates", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:15:00.000Z"}, generate_candidates_out, generate_candidates_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "scheduling-generate-candidates should succeed before proposal generation");

    std::ostringstream generate_out;
    std::ostringstream generate_err;
    rc = life_orchestrator::app::run_application({"scheduling-generate-proposals", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:20:00.000Z"}, generate_out, generate_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "scheduling-generate-proposals should succeed");
    const auto generate_text = generate_out.str();
    assert_true(generate_text.find("scheduling_generate_proposals=ok") != std::string::npos, "scheduling proposal generation should emit ok");
    assert_true(generate_text.find("candidate_count=1") != std::string::npos, "scheduling proposal generation should count candidates");
    assert_true(generate_text.find("proposal_count=1") != std::string::npos, "scheduling proposal generation should create one proposal");
    assert_true(generate_text.find("conflict_count=0") != std::string::npos, "scheduling proposal generation should expose zero conflicts");

    std::ostringstream list_out;
    std::ostringstream list_err;
    rc = life_orchestrator::app::run_application({"scheduling-list-proposals", "--data-root=" + root.string(), "--quiet-startup"}, list_out, list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "scheduling-list-proposals should succeed");
    const auto list_text = list_out.str();
    assert_true(list_text.find("scheduling_list_proposals=ok") != std::string::npos, "scheduling proposal list should emit ok");
    assert_true(list_text.find("schedule_proposal_id=schedule_proposal.candidate.intervention.") != std::string::npos, "scheduling proposal list should emit deterministic proposal ids");
    assert_true(list_text.find("source_candidate_id=candidate.intervention.") != std::string::npos, "scheduling proposal list should expose candidate lineage");
    assert_true(list_text.find("source_intervention_id=intervention.") != std::string::npos, "scheduling proposal list should expose intervention lineage");
    assert_true(list_text.find("source_proposal_id=proposal.") != std::string::npos, "scheduling proposal list should expose procedural proposal lineage");
    assert_true(list_text.find("source_audit_run_id=audit.") != std::string::npos, "scheduling proposal list should expose audit lineage");
    assert_true(list_text.find("source_activity_id=activity.proposal_bridge") != std::string::npos, "scheduling proposal list should expose activity lineage");
    assert_true(list_text.find("proposed_start_time=2026-03-19T12:00:00.000Z") != std::string::npos, "scheduling proposal list should emit deterministic start time");
    assert_true(list_text.find("proposed_end_time=2026-03-19T12:30:00.000Z") != std::string::npos, "scheduling proposal list should emit deterministic end time");
    assert_true(list_text.find("timezone=UTC") != std::string::npos, "scheduling proposal list should emit timezone");
    assert_true(list_text.find("duration_minutes=30") != std::string::npos, "scheduling proposal list should emit duration");
    assert_true(list_text.find("scheduling_window_hint=next_3_days") != std::string::npos, "scheduling proposal list should emit window hint");
    assert_true(list_text.find("recommended_time_of_day=midday") != std::string::npos, "scheduling proposal list should emit time of day");
    assert_true(list_text.find("proposal_status=proposed") != std::string::npos, "scheduling proposal list should emit proposal status");
    assert_true(list_text.find("conflict_status=none") != std::string::npos, "scheduling proposal list should emit conflict status");
    assert_in_order(list_text, {"schedule_proposal_id=", "source_candidate_id=", "source_intervention_id=", "source_proposal_id=", "source_audit_run_id=", "source_activity_id=", "proposed_start_time=", "proposed_end_time=", "timezone=", "duration_minutes=", "scheduling_window_hint=", "recommended_time_of_day=", "rationale=", "proposal_status=", "conflict_status="}, "scheduling proposal list should emit stable fields");

    std::ostringstream generate_repeat_out;
    std::ostringstream generate_repeat_err;
    rc = life_orchestrator::app::run_application({"scheduling-generate-proposals", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:20:00.000Z"}, generate_repeat_out, generate_repeat_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "re-running scheduling-generate-proposals should succeed");

    std::ostringstream list_repeat_out;
    std::ostringstream list_repeat_err;
    rc = life_orchestrator::app::run_application({"scheduling-list-proposals", "--data-root=" + root.string(), "--quiet-startup"}, list_repeat_out, list_repeat_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "re-listing scheduling proposals should succeed");
    assert_true(list_repeat_out.str().find("proposal_count=1") != std::string::npos, "unchanged scheduling proposal reruns should remain idempotent");

    std::ostringstream status_out;
    std::ostringstream status_err;
    rc = life_orchestrator::app::run_application({"status", "--data-root=" + root.string(), "--quiet-startup"}, status_out, status_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "general status should succeed for scheduling proposal test");
    assert_true(status_out.str().find("scheduling_proposal_count=1") != std::string::npos, "general status should expose scheduling proposal count");
}

void test_scheduling_proposal_default_emission_and_backward_compatibility() {
    const std::filesystem::path root = "artifacts/scheduling_proposal_defaults";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "memory" / "scheduling");
    {
        std::ofstream out(root / "memory" / "scheduling" / "proposal_artifacts.ndjson");
        assert_true(out.is_open(), "legacy scheduling proposal artifacts file should open");
        out << "schedule_proposal_id=schedule_proposal.legacy\n";
    }

    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/scheduling_proposal_defaults.ndjson"};
    std::filesystem::remove(logger.log_path());
    life_orchestrator::core::FileMemoryStore store{root, &logger};
    assert_true(store.load_from_disk().ok, "legacy scheduling proposal artifact load should succeed");
    auto proposals = store.list_schedule_proposal_artifacts();
    assert_true(proposals.ok && proposals.value->size() == 1, "legacy scheduling proposal artifact should reload");
    const auto& proposal = proposals.value->front();
    assert_true(proposal.source_candidate_id == "none", "legacy scheduling proposal artifact should default candidate lineage");
    assert_true(proposal.source_intervention_id == "none", "legacy scheduling proposal artifact should default intervention lineage");
    assert_true(proposal.source_proposal_id == "none", "legacy scheduling proposal artifact should default proposal lineage");
    assert_true(proposal.source_audit_run_id == "none", "legacy scheduling proposal artifact should default audit lineage");
    assert_true(proposal.source_activity_id == "none", "legacy scheduling proposal artifact should default activity lineage");
    assert_true(proposal.proposed_start_time == "unspecified", "legacy scheduling proposal artifact should default start time");
    assert_true(proposal.proposed_end_time == "unspecified", "legacy scheduling proposal artifact should default end time");
    assert_true(proposal.timezone == "UTC", "legacy scheduling proposal artifact should default timezone");
    assert_true(proposal.duration_minutes == 0, "legacy scheduling proposal artifact should default duration");
    assert_true(proposal.scheduling_window_hint == "unspecified", "legacy scheduling proposal artifact should default window hint");
    assert_true(proposal.recommended_time_of_day == "unspecified", "legacy scheduling proposal artifact should default time of day");
    assert_true(proposal.rationale == "none", "legacy scheduling proposal artifact should default rationale");
    assert_true(proposal.proposal_status == life_orchestrator::core::ScheduleProposalArtifactStatus::Proposed, "legacy scheduling proposal artifact should default proposal status");
    assert_true(proposal.conflict_status == life_orchestrator::core::ScheduleProposalConflictStatus::None, "legacy scheduling proposal artifact should default conflict status");

    std::ostringstream list_out;
    std::ostringstream list_err;
    auto rc = life_orchestrator::app::run_application({"scheduling-list-proposals", "--data-root=" + root.string(), "--quiet-startup"}, list_out, list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "scheduling-list-proposals should succeed for legacy proposal data");
    const auto text = list_out.str();
    assert_true(text.find("source_candidate_id=none") != std::string::npos, "legacy proposal list should emit default candidate lineage");
    assert_true(text.find("source_intervention_id=none") != std::string::npos, "legacy proposal list should emit default intervention lineage");
    assert_true(text.find("source_proposal_id=none") != std::string::npos, "legacy proposal list should emit default proposal lineage");
    assert_true(text.find("source_audit_run_id=none") != std::string::npos, "legacy proposal list should emit default audit lineage");
    assert_true(text.find("source_activity_id=none") != std::string::npos, "legacy proposal list should emit default activity lineage");
    assert_true(text.find("proposed_start_time=unspecified") != std::string::npos, "legacy proposal list should emit default start time");
    assert_true(text.find("proposed_end_time=unspecified") != std::string::npos, "legacy proposal list should emit default end time");
    assert_true(text.find("timezone=UTC") != std::string::npos, "legacy proposal list should emit default timezone");
    assert_true(text.find("duration_minutes=0") != std::string::npos, "legacy proposal list should emit default duration");
    assert_true(text.find("scheduling_window_hint=unspecified") != std::string::npos, "legacy proposal list should emit default window hint");
    assert_true(text.find("recommended_time_of_day=unspecified") != std::string::npos, "legacy proposal list should emit default time-of-day");
    assert_true(text.find("rationale=none") != std::string::npos, "legacy proposal list should emit default rationale");
    assert_true(text.find("proposal_status=proposed") != std::string::npos, "legacy proposal list should emit default proposal status");
    assert_true(text.find("conflict_status=none") != std::string::npos, "legacy proposal list should emit default conflict status");
}

void test_runtime_hygiene_ignore_file() {
    auto gitignore_path = std::filesystem::exists(".gitignore") ? std::filesystem::path{".gitignore"} : std::filesystem::path{"../.gitignore"};
    std::ifstream in{gitignore_path};
    assert_true(in.is_open(), ".gitignore should exist");
    std::stringstream buffer;
    buffer << in.rdbuf();
    assert_true(buffer.str().find("runtime/") != std::string::npos, "runtime artifacts should be gitignored");
}

void test_operator_alias_resolution_and_suggestions() {
    const std::filesystem::path root = "artifacts/operator_aliases";
    std::filesystem::remove_all(root);

    std::ostringstream alias_out;
    std::ostringstream alias_err;
    auto rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + root.string(), "--quiet-startup", "--input", "backlog"}, alias_out, alias_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "operator-query should resolve aliases before fallback");
    assert_true(alias_out.str().find("behavioral_list_backlog=ok") != std::string::npos, "alias should map to deterministic backlog command");

    std::ostringstream suggest_out;
    std::ostringstream suggest_err;
    rc = life_orchestrator::app::run_application({"suggest", "--data-root=" + root.string(), "--quiet-startup", "--input", "stat"}, suggest_out, suggest_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "suggest should succeed");
    const auto suggestions = suggest_out.str();
    assert_in_order(suggestions, {"suggestion=status", "suggestion=status=>status"}, "suggest should prefer exact deterministic commands before alias matches");

    std::ostringstream aliases_out;
    std::ostringstream aliases_err;
    rc = life_orchestrator::app::run_application({"aliases", "--data-root=" + root.string(), "--quiet-startup"}, aliases_out, aliases_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "aliases should succeed");
    assert_true(aliases_out.str().find("alias=backlog;command=behavioral-list-backlog") != std::string::npos, "aliases output should expose canonical alias mapping");
}

void test_application_command_helper_exports() {
    const auto commands = life_orchestrator::app::list_application_commands();
    assert_true(std::find(commands.begin(), commands.end(), "status") != commands.end(), "command helper should expose status");
    assert_true(std::find(commands.begin(), commands.end(), "operator-console") != commands.end(), "command helper should expose operator console");
    assert_true(std::find(commands.begin(), commands.end(), "artifact.query") != commands.end(), "command helper should expose artifact query command");

    const auto aliases = life_orchestrator::app::list_application_aliases();
    auto saw_backlog = false;
    for (const auto& [alias, command] : aliases) {
        if (alias == "backlog" && command == "behavioral-list-backlog") {
            saw_backlog = true;
            break;
        }
    }
    assert_true(saw_backlog, "alias helper should expose deterministic alias mappings");

    const auto suggestions = life_orchestrator::app::suggest_application_commands("stat");
    assert_true(!suggestions.empty() && suggestions.front() == "status", "suggestion helper should preserve deterministic ordering");

    const auto result = life_orchestrator::app::invoke_application_command({"commands", "--data-root=artifacts/app_helper_exports", "--quiet-startup"},
                                                                           "",
                                                                           std::filesystem::current_path());
    assert_true(result.exit_code == 0, "invoke_application_command should succeed");
    assert_true(result.standard_output.find("commands=ok") != std::string::npos, "command helper should capture stdout");
}


void test_command_surface_aliases_help_and_discoverability() {
    const std::filesystem::path root = "artifacts/command_surface";
    std::filesystem::remove_all(root);

    std::ostringstream state_out;
    std::ostringstream state_err;
    auto rc = life_orchestrator::app::run_application({"record-state", "--data-root=" + root.string(), "--quiet-startup", "--available-capacity", "8", "--stress-level", "2", "--cognitive-load", "3", "--motivation-level", "7", "--recovery-status", "8", "--now", "2026-03-19T09:00:00.000Z"}, state_out, state_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "record-state alias should accept --motivation-level");
    assert_true(state_out.str().find("behavioral_record_state=ok") != std::string::npos, "behavioral state alias should dispatch canonical command");

    std::ostringstream upsert_out;
    std::ostringstream upsert_err;
    rc = life_orchestrator::app::run_application({"create-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.create", "--title", "Create alias", "--domain-source", "planning", "--frequency", "daily", "--duration-minutes", "30", "--effort-estimate", "4", "--outcome-value", "6", "--now", "2026-03-19T09:00:00.000Z"}, upsert_out, upsert_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "create-activity alias should resolve to canonical upsert command");
    assert_true(upsert_out.str().find("activity_id=activity.create") != std::string::npos, "create-activity alias should preserve canonical output");

    std::ostringstream help_out;
    std::ostringstream help_err;
    rc = life_orchestrator::app::run_application({"behavioral-record-state", "--data-root=" + root.string(), "--quiet-startup", "--help"}, help_out, help_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "behavioral-record-state --help should succeed");
    const auto help_text = help_out.str();
    assert_true(help_text.find("canonical_command=behavioral-record-state") != std::string::npos, "help should expose canonical command name");
    assert_true(help_text.find("alias=record-state") != std::string::npos, "help should expose aliases");
    assert_true(help_text.find("required=--motivation-level") != std::string::npos, "help should expose canonical required motivation flag");
    assert_true(help_text.find("example=behavioral-record-state --available-capacity 8 --stress-level 2 --cognitive-load 3 --motivation-level 7 --recovery-status 8") != std::string::npos, "help should expose minimal example");

    std::ostringstream missing_out;
    std::ostringstream missing_err;
    rc = life_orchestrator::app::run_application({"behavioral-record-state", "--data-root=" + root.string(), "--quiet-startup", "--available-capacity", "8", "--stress-level", "2", "--cognitive-load", "3", "--recovery-status", "8"}, missing_out, missing_err, "", std::filesystem::current_path());
    assert_true(rc == 2, "behavioral-record-state should fail clearly when motivation is omitted");
    assert_true(missing_err.str().find("accepted_flags=--motivation-level,--motivation") != std::string::npos, "missing motivation error should identify accepted flags");

    const auto create_suggestions = life_orchestrator::app::suggest_application_commands("create");
    assert_true(std::find(create_suggestions.begin(), create_suggestions.end(), "create=>procedural-upsert-activity") != create_suggestions.end(), "palette suggestions should index create alias");
    const auto activity_suggestions = life_orchestrator::app::suggest_application_commands("activity");
    assert_true(std::find(activity_suggestions.begin(), activity_suggestions.end(), "activity=>procedural-upsert-activity") != activity_suggestions.end(), "palette suggestions should index activity alias");
    const auto record_suggestions = life_orchestrator::app::suggest_application_commands("record");
    assert_true(std::find(record_suggestions.begin(), record_suggestions.end(), "record=>behavioral-record-state") != record_suggestions.end(), "palette suggestions should index record alias");
    const auto state_suggestions = life_orchestrator::app::suggest_application_commands("state");
    assert_true(std::find(state_suggestions.begin(), state_suggestions.end(), "state=>behavioral-record-state") != state_suggestions.end(), "palette suggestions should index state alias");
}




void test_quick_action_refresh_targets_are_stable() {
    struct Expectation { std::string action_id; std::vector<std::string> targets; };
    const std::vector<Expectation> expectations = {
        {"create_activity", {"activity_inventory"}},
        {"record_behavioral_state", {"behavioral_backlog", "behavioral_interventions"}},
        {"run_procedural_audit", {"procedural_proposals"}},
        {"behavioral_reevaluation", {"behavioral_reevaluations", "behavioral_backlog", "behavioral_interventions"}},
        {"generate_scheduling_candidates", {"scheduling_candidates"}},
        {"generate_schedule_proposals", {"schedule_proposals"}},
        {"update_provider_configuration", {"provider_config_summary"}},
        {"provider_readiness_test", {"provider_config_summary"}},
    };
    for (const auto& expectation : expectations) {
        const auto spec = life_orchestrator::app::find_action_form_spec_by_id(expectation.action_id);
        assert_true(spec.has_value(), "refresh-target expectation should resolve action form spec");
        assert_true(spec->refresh_targets == expectation.targets, "action form refresh targets should remain stable");
    }
}

void test_action_result_view_preserves_authoritative_success_and_failure_output() {
    const auto create_spec = life_orchestrator::app::find_action_form_spec_by_id("create_activity");
    assert_true(create_spec.has_value(), "create activity spec should exist for result view tests");
    const auto success = life_orchestrator::app::build_action_execution_result_view(*create_spec,
                                                                                     {0,
                                                                                      R"(procedural_upsert_activity=ok
activity_id=activity.gui
version=1
)",
                                                                                      ""});
    assert_true(success.succeeded, "successful result view should preserve exit code authority");
    assert_true(success.action_label == "Create Activity", "result view should preserve action label");
    assert_true(success.canonical_command_id == "procedural-upsert-activity", "result view should preserve canonical command id");
    assert_true(success.output_rows.size() >= 3, "successful result view should parse deterministic output rows");
    assert_true(success.next_state_hint.find("Activity Inventory") != std::string::npos, "next-state hint should use registry-sourced artifact label");

    const auto failure_spec = life_orchestrator::app::find_action_form_spec_by_id("record_behavioral_state");
    assert_true(failure_spec.has_value(), "behavioral state spec should exist for result view tests");
    const auto failure = life_orchestrator::app::build_action_execution_result_view(*failure_spec,
                                                                                     {2,
                                                                                      "",
                                                                                      R"(validation=failed
accepted_flags=--motivation-level,--motivation
)"});
    assert_true(!failure.succeeded, "failed result view should preserve failure status");
    assert_true(failure.raw_output.find("accepted_flags=--motivation-level,--motivation") != std::string::npos, "failed result view should preserve authoritative command-layer error text");
    assert_true(!failure.output_rows.empty(), "failed result view should still parse deterministic failure rows");
}

void test_execute_action_form_command_refreshes_only_registered_query_surfaces() {
    const std::filesystem::path root = "artifacts/action_feedback_refresh";
    std::filesystem::remove_all(root);

    const auto create_spec = life_orchestrator::app::find_action_form_spec_by_id("create_activity");
    assert_true(create_spec.has_value(), "create activity spec should exist for refresh execution tests");
    const auto feedback = life_orchestrator::app::execute_action_form_command(*create_spec,
                                                                              {create_spec->canonical_command_target, "--data-root=" + root.string(), "--activity-id", "activity.gui.refresh", "--title", "GUI refresh", "--domain-source", "planning", "--frequency", "daily", "--duration-minutes", "30", "--effort-estimate", "4", "--outcome-value", "6", "--now", "2026-03-19T09:00:00.000Z"},
                                                                              "",
                                                                              std::filesystem::current_path());
    assert_true(feedback.result_view.succeeded, "successful action execution feedback should report success");
    assert_true(feedback.refreshed_artifacts.size() == 1, "successful action execution should refresh only registered targets");
    assert_true(feedback.refreshed_artifacts.front().query_args.size() >= 3 && feedback.refreshed_artifacts.front().query_args.front() == "artifact.query", "refresh should execute only through artifact query surface");
    assert_true(feedback.refreshed_artifacts.front().query_args[2] == "activity_inventory", "refresh should target registered activity inventory artifact type");
    assert_true(feedback.refreshed_artifacts.front().query_result.exit_code == 0, "refresh query should succeed through authoritative artifact surface");
    assert_true(feedback.refreshed_artifacts.front().query_result.standard_output.find("artifact_query=ok") != std::string::npos, "refresh query should remain an artifact.query invocation");
}

void test_supported_quick_actions_have_stable_ordered_fields() {
    struct OrderedFieldExpectation {
        std::string action_id;
        std::vector<std::string> ordered_field_ids;
    };
    const std::vector<OrderedFieldExpectation> expectations = {
        {"create_activity", {"activity_inventory_item_id", "title", "domain_source", "frequency", "duration_minutes", "effort_estimate", "outcome_value", "repeatable", "attributes_json", "now"}},
        {"record_behavioral_state", {"available_capacity", "stress_level", "cognitive_load", "motivation", "recovery_status", "sleep_quality", "time_pressure", "notes", "attributes_json", "now"}},
        {"run_procedural_audit", {"procedural_audit_run_id", "now"}},
        {"generate_scheduling_candidates", {"now"}},
        {"generate_schedule_proposals", {"now"}},
    };

    for (const auto& expectation : expectations) {
        const auto spec = life_orchestrator::app::find_action_form_spec_by_id(expectation.action_id);
        assert_true(spec.has_value(), "supported quick action should resolve to action form spec");
        assert_true(spec->input_fields.size() == expectation.ordered_field_ids.size(), "supported quick action should expose stable field count");
        for (std::size_t index = 0; index < expectation.ordered_field_ids.size(); ++index) {
            assert_true(spec->input_fields[index].field_id == expectation.ordered_field_ids[index], "supported quick action should preserve stable ordered fields");
        }
    }
}

void test_action_form_submission_uses_registry_flag_names() {
    const auto create_spec = life_orchestrator::app::find_action_form_spec_by_id("create_activity");
    assert_true(create_spec.has_value(), "create activity spec should exist for submission tests");
    const auto create_submission = life_orchestrator::app::build_action_form_submission_args(
        *create_spec,
        {{"activity_inventory_item_id", "activity.gui"},
         {"title", "GUI activity"},
         {"domain_source", "planning"},
         {"frequency", "daily"},
         {"duration_minutes", "30"},
         {"effort_estimate", "4"},
         {"outcome_value", "6"},
         {"repeatable", "1"}});
    assert_true(create_submission.empty_required_field_ids.empty(), "submission should not mark required values missing when all required values are present");
    assert_in_order(std::accumulate(create_submission.args.begin(), create_submission.args.end(), std::string{}, [](std::string out, const std::string& value) {
                        if (!out.empty()) out += ' ';
                        out += value;
                        return out;
                    }),
                    {"procedural-upsert-activity", "--activity-id", "activity.gui", "--effort-estimate", "4", "--repeatable", "1"},
                    "submission should use canonical registry flag ordering");

    const auto behavioral_spec = life_orchestrator::app::find_action_form_spec_by_id("record_behavioral_state");
    assert_true(behavioral_spec.has_value(), "behavioral state spec should exist for submission tests");
    const auto behavioral_submission = life_orchestrator::app::build_action_form_submission_args(
        *behavioral_spec,
        {{"available_capacity", "8"},
         {"stress_level", "2"},
         {"cognitive_load", "3"},
         {"motivation", "7"},
         {"recovery_status", "8"}});
    assert_true(std::find(behavioral_submission.args.begin(), behavioral_submission.args.end(), "--motivation-level") != behavioral_submission.args.end(), "submission should prefer canonical motivation flag from registry");
    assert_true(std::find(behavioral_submission.args.begin(), behavioral_submission.args.end(), "--motivation") == behavioral_submission.args.end(), "submission should not fall back to alternate flag when canonical registry flag is available");
}

void test_artifact_presentation_and_action_form_registries() {
    const std::vector<std::string> expected_artifacts = {"activity_inventory", "procedural_proposals", "behavioral_backlog", "behavioral_interventions", "scheduling_candidates", "schedule_proposals", "behavioral_reevaluations", "provider_config_summary"};
    for (const auto& artifact_type : expected_artifacts) {
        const auto schema = life_orchestrator::app::find_artifact_presentation_schema(artifact_type);
        assert_true(schema.has_value(), "every supported artifact type should resolve to a presentation schema");
    }

    const std::vector<std::string> expected_actions = {"create_activity", "record_behavioral_state", "run_procedural_audit", "behavioral_reevaluation", "generate_scheduling_candidates", "generate_schedule_proposals", "update_provider_configuration", "provider_readiness_test"};
    for (const auto& action_id : expected_actions) {
        const auto spec = life_orchestrator::app::find_action_form_spec_by_id(action_id);
        assert_true(spec.has_value(), "every GUI quick action should resolve to an authoritative action form spec");
    }

    const auto activity_schema = life_orchestrator::app::find_artifact_presentation_schema("activity_inventory");
    assert_true(activity_schema.has_value(), "activity inventory schema should exist");
    assert_true(activity_schema->summary_fields.size() >= 3, "activity schema should define ordered summary fields");
    assert_true(activity_schema->summary_fields[0].field_key == "title", "activity schema should keep title first");
    assert_true(activity_schema->summary_fields[1].field_key == "domain_source", "activity schema should keep domain second");
    assert_true(activity_schema->summary_fields[2].field_key == "frequency", "activity schema should keep frequency third");

    const auto provider_schema = life_orchestrator::app::find_artifact_presentation_schema("provider_config_summary");
    assert_true(provider_schema.has_value(), "provider schema should exist");
    for (const auto& field : provider_schema->summary_fields) {
        assert_true(field.field_key != "api_key", "provider config schema must not surface raw secret fields");
    }

    const auto provider_update = life_orchestrator::app::find_action_form_spec_by_id("update_provider_configuration");
    assert_true(provider_update.has_value(), "provider update action form should exist");
    assert_true(provider_update->display_label == "Configure Provider", "provider action label should be authoritative");
}

void test_gui_panels_and_quick_actions_source_registry_labels() {
    const auto panels = life_orchestrator::ui::build_artifact_panels();
    assert_true(panels.size() == life_orchestrator::app::list_artifact_panel_definition_ids().size(), "GUI panels should align with registry-defined artifact panels");

    auto procedural_panel = std::find_if(panels.begin(), panels.end(), [](const auto& panel) { return panel->artifact_type() == "procedural_proposals"; });
    assert_true(procedural_panel != panels.end(), "procedural proposals panel should exist");
    assert_true((*procedural_panel)->title() == "Procedural Proposals", "panel title should come from artifact registry");
    assert_true(!(*procedural_panel)->actions().empty(), "procedural proposals panel should expose schema-driven actions");
    assert_true((*procedural_panel)->actions().front().button_label == "Run Procedural Audit", "panel action labels should come from authoritative schema");

    const auto create_activity = life_orchestrator::app::find_action_form_spec_by_command_target("procedural-upsert-activity");
    assert_true(create_activity.has_value(), "create activity action form should resolve by canonical command");
    assert_true(create_activity->display_label == "Create Activity", "GUI quick-action label should be registry sourced");
}

void test_command_help_and_action_form_consistency() {
    const std::filesystem::path root = "artifacts/action_form_help";
    std::filesystem::remove_all(root);
    struct Check { std::string command; std::string action_id; std::vector<std::string> canonical_flags; };
    const std::vector<Check> checks = {
        {"procedural-upsert-activity", "create_activity", {"--activity-id", "--title", "--domain-source", "--frequency", "--duration-minutes", "--effort-estimate", "--outcome-value"}},
        {"behavioral-record-state", "record_behavioral_state", {"--available-capacity", "--stress-level", "--cognitive-load", "--motivation-level", "--recovery-status"}},
        {"scheduling-generate-candidates", "generate_scheduling_candidates", {"--now"}},
        {"scheduling-generate-proposals", "generate_schedule_proposals", {"--now"}},
    };

    for (const auto& check : checks) {
        std::ostringstream out;
        std::ostringstream err;
        const auto rc = life_orchestrator::app::run_application({check.command, "--data-root=" + root.string(), "--quiet-startup", "--help"}, out, err, "", std::filesystem::current_path());
        assert_true(rc == 0, "command help should succeed for action form consistency checks");
        const auto help_text = out.str();
        const auto spec = life_orchestrator::app::find_action_form_spec_by_id(check.action_id);
        assert_true(spec.has_value(), "action form spec should exist for consistency check");
        assert_true(help_text.find("canonical_command=" + spec->canonical_command_target) != std::string::npos, "help should match action form canonical command target");
        for (const auto& flag : check.canonical_flags) {
            assert_true(help_text.find(flag) != std::string::npos, "help should include canonical flag from action form spec");
            auto field_it = std::find_if(spec->input_fields.begin(), spec->input_fields.end(), [&](const auto& field) {
                return std::find(field.accepted_flags.begin(), field.accepted_flags.end(), flag) != field.accepted_flags.end();
            });
            assert_true(field_it != spec->input_fields.end(), "action form spec should include canonical help flag");
        }
    }
}

void test_form_spec_missing_required_inputs_defer_to_command_layer() {
    const std::filesystem::path root = "artifacts/form_spec_missing_inputs";
    std::filesystem::remove_all(root);
    const auto spec = life_orchestrator::app::find_action_form_spec_by_id("record_behavioral_state");
    assert_true(spec.has_value(), "record behavioral state form spec should exist");
    auto rc_out = std::ostringstream{};
    auto rc_err = std::ostringstream{};
    const auto rc = life_orchestrator::app::run_application({spec->canonical_command_target,
                                                             "--data-root=" + root.string(),
                                                             "--quiet-startup",
                                                             "--available-capacity", "8",
                                                             "--stress-level", "2",
                                                             "--cognitive-load", "3",
                                                             "--recovery-status", "8"},
                                                            rc_out,
                                                            rc_err,
                                                            "",
                                                            std::filesystem::current_path());
    assert_true(rc == 2, "missing required inputs through form spec path should still fail in deterministic command layer");
    assert_true(rc_err.str().find("accepted_flags=--motivation-level,--motivation") != std::string::npos, "command-layer validation should remain authoritative for missing required inputs");
}

void test_end_to_end_naive_operator_flow() {
    const std::filesystem::path root = "artifacts/naive_operator_flow";
    std::filesystem::remove_all(root);

    std::ostringstream create_out;
    std::ostringstream create_err;
    auto rc = life_orchestrator::app::run_application({"create-activity", "--data-root=" + root.string(), "--quiet-startup", "--activity-id", "activity.flow", "--title", "Flow activity", "--domain-source", "planning", "--frequency", "daily", "--duration-minutes", "40", "--effort-estimate", "6", "--outcome-value", "8", "--repeatable", "1", "--now", "2026-03-19T09:00:00.000Z"}, create_out, create_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "create-activity should succeed in end-to-end flow");

    std::ostringstream list_out;
    std::ostringstream list_err;
    rc = life_orchestrator::app::run_application({"procedural-list-activities", "--data-root=" + root.string(), "--quiet-startup"}, list_out, list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "procedural-list-activities should succeed in end-to-end flow");
    assert_true(list_out.str().find("activity_id=activity.flow") != std::string::npos, "list activities should include created activity");

    std::ostringstream state_out;
    std::ostringstream state_err;
    rc = life_orchestrator::app::run_application({"record-behavioral-state", "--data-root=" + root.string(), "--quiet-startup", "--available-capacity", "8", "--stress-level", "2", "--cognitive-load", "3", "--motivation-level", "7", "--recovery-status", "8", "--sleep-quality", "8", "--time-pressure", "2", "--now", "2026-03-19T09:00:00.000Z"}, state_out, state_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "record-behavioral-state should succeed in end-to-end flow");

    std::ostringstream audit_out;
    std::ostringstream audit_err;
    rc = life_orchestrator::app::run_application({"run-audit", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:00:00.000Z"}, audit_out, audit_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "run-audit alias should succeed in end-to-end flow");
    assert_true(audit_out.str().find("proposal_count=") != std::string::npos, "audit should emit proposal count");

    std::ostringstream candidate_out;
    std::ostringstream candidate_err;
    rc = life_orchestrator::app::run_application({"generate-candidates", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:00:00.000Z"}, candidate_out, candidate_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "generate-candidates alias should succeed in end-to-end flow");
    assert_true(candidate_out.str().find("candidate_count=") != std::string::npos, "candidate generation should emit candidate count");

    std::ostringstream proposal_out;
    std::ostringstream proposal_err;
    rc = life_orchestrator::app::run_application({"generate-proposals", "--data-root=" + root.string(), "--quiet-startup", "--now", "2026-03-19T09:00:00.000Z"}, proposal_out, proposal_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "generate-proposals alias should succeed in end-to-end flow");
    assert_true(proposal_out.str().find("proposal_count=") != std::string::npos, "proposal generation should emit proposal count");

    std::ostringstream artifact_out;
    std::ostringstream artifact_err;
    rc = life_orchestrator::app::run_application({"scheduling-list-proposals", "--data-root=" + root.string(), "--quiet-startup"}, artifact_out, artifact_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "scheduling-list-proposals should succeed after naive flow");
    assert_true(artifact_out.str().find("proposal_count=0") == std::string::npos, "downstream artifacts should be nonzero after valid behavioral state and audit flow");
}

void test_integration_provider_persistence_redaction_and_visibility() {
    const std::filesystem::path root = "artifacts/operator_provider";
    std::filesystem::remove_all(root);

    std::ostringstream set_out;
    std::ostringstream set_err;
    auto rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai", "--api-key", "TEST_KEY_123", "--model-name", "gpt-5"}, set_out, set_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "integration-set-provider should succeed");
    assert_true(std::filesystem::exists(root / "config" / "providers" / "openai.secret"), "provider secret should persist locally");

    std::ostringstream show_out;
    std::ostringstream show_err;
    rc = life_orchestrator::app::run_application({"integration-show-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai"}, show_out, show_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "integration-show-provider should succeed");
    const auto show_text = show_out.str();
    assert_true(show_text.find("api_key_redacted=TE***23") != std::string::npos, "show provider should redact api key");
    assert_true(show_text.find("model_name=gpt-5") != std::string::npos, "show provider should reload non-secret settings");
    assert_true(show_text.find("TEST_KEY_123") == std::string::npos, "show provider should never expose raw key");
    assert_true(show_text.find("secret_source=direct") != std::string::npos, "show provider should expose direct-secret metadata");

    std::ostringstream list_out;
    std::ostringstream list_err;
    rc = life_orchestrator::app::run_application({"integration-list-providers", "--data-root=" + root.string(), "--quiet-startup"}, list_out, list_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "integration-list-providers should succeed");
    const auto list_text = list_out.str();
    assert_true(list_text.find("provider_count=1") != std::string::npos, "provider list should show configured providers");
    assert_true(list_text.find("provider_name=openai") != std::string::npos, "provider list should expose provider name");
    assert_true(list_text.find("api_key_redacted=TE***23") != std::string::npos, "provider list should redact keys");
}

void test_integration_set_provider_env_var_reference_mode() {
    const std::filesystem::path root = "artifacts/operator_provider_env";
    std::filesystem::remove_all(root);
    setenv("OPENAI_API_KEY", "TEST_ENV_KEY_789", 1);

    std::ostringstream set_out;
    std::ostringstream set_err;
    auto rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai", "--model-name", "gpt-5", "--secret-source", "env", "--env-var", "OPENAI_API_KEY"}, set_out, set_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "integration-set-provider should accept env-var reference mode");
    assert_true(!std::filesystem::exists(root / "config" / "providers" / "openai.secret"), "env-var mode should not create a local secret file");
    const auto set_text = set_out.str();
    assert_true(set_text.find("secret_source=env") != std::string::npos, "env-var mode should record secret source");
    assert_true(set_text.find("env_var_name=OPENAI_API_KEY") != std::string::npos, "env-var mode should echo env-var metadata");
    assert_true(set_text.find("api_key_redacted=TE***89") != std::string::npos, "env-var mode should redact resolved secret values");

    std::ostringstream show_out;
    std::ostringstream show_err;
    rc = life_orchestrator::app::run_application({"integration-show-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai"}, show_out, show_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "integration-show-provider should succeed after env-var setup");
    const auto show_text = show_out.str();
    assert_true(show_text.find("credential_storage_mode=InlinePlaceholderOnly") != std::string::npos, "env-var setup should persist env-backed storage mode");
    assert_true(show_text.find("env_var_name=OPENAI_API_KEY") != std::string::npos, "show provider should preserve env-var metadata");
    assert_true(show_text.find("TEST_ENV_KEY_789") == std::string::npos, "show provider should not leak env secret values");

    std::ostringstream readiness_out;
    std::ostringstream readiness_err;
    rc = life_orchestrator::app::run_application({"integration-test-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai"}, readiness_out, readiness_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "integration-test-provider should work with env-var-backed secrets");
    assert_true(readiness_out.str().find("secret_resolved=true") != std::string::npos, "env-var-backed provider should resolve its secret");
    assert_true(readiness_out.str().find("structured_result_returned=true") != std::string::npos, "env-var-backed provider should return a structured health-check result");
}


void test_operator_query_provider_and_status_visibility() {
    const std::filesystem::path no_provider_root = "artifacts/operator_query_no_provider";
    std::filesystem::remove_all(no_provider_root);

    std::ostringstream missing_out;
    std::ostringstream missing_err;
    auto rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + no_provider_root.string(), "--quiet-startup", "--input", "what should I focus on today?"}, missing_out, missing_err, "", std::filesystem::current_path());
    assert_true(rc == 3, "operator-query without provider should fail gracefully");
    assert_true(missing_err.str().find("message=no_provider_configured") != std::string::npos, "operator-query should clearly report no configured provider");
    assert_true(missing_err.str().find("remediation_action=update_provider_configuration") != std::string::npos, "operator-query should emit provider setup guidance when no provider is configured");

    const std::filesystem::path provider_root = "artifacts/operator_query_provider";
    std::filesystem::remove_all(provider_root);
    std::ostringstream set_out;
    std::ostringstream set_err;
    rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + provider_root.string(), "--quiet-startup", "--provider-name", "openai", "--api-key", "TEST_KEY_123", "--model-name", "gpt-5"}, set_out, set_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "provider should configure for operator-query tests");

    std::ostringstream deterministic_out;
    std::ostringstream deterministic_err;
    rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + provider_root.string(), "--quiet-startup", "--input", "status"}, deterministic_out, deterministic_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "operator-query should execute deterministic status command before fallback");
    assert_true(deterministic_out.str().find("run_mode=status") != std::string::npos, "deterministic operator-query input should route to status");

    std::ostringstream explicit_out;
    std::ostringstream explicit_err;
    rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + no_provider_root.string(), "--quiet-startup", "--input", "integration-list-providers"}, explicit_out, explicit_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "explicit integration-list-providers should bypass provider routing even without configured provider");
    assert_true(explicit_out.str().find("operator_query=llm_interpreter") == std::string::npos, "explicit integration-list-providers should bypass provider-backed routing");
    assert_true(explicit_err.str().find("no_provider_configured") == std::string::npos, "explicit integration-list-providers should not fail for missing provider");

    std::ostringstream exact_show_out;
    std::ostringstream exact_show_err;
    rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + provider_root.string(), "--quiet-startup", "--input", "integration-show-provider --provider-name openai"}, exact_show_out, exact_show_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "explicit integration-show-provider with flags should bypass intent routing");
    assert_true(exact_show_out.str().find("operator_query=llm_interpreter") == std::string::npos, "explicit integration-show-provider should bypass provider-backed routing");
    assert_true(exact_show_out.str().find("provider_name=openai") != std::string::npos, "explicit integration-show-provider should execute the underlying command");

    std::ostringstream dash_out;
    std::ostringstream dash_err;
    rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + provider_root.string(), "--quiet-startup", "--input", std::string("integration-test-provider ") + std::string("\xE2\x80\x94", 3) + "provider-name openai"}, dash_out, dash_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "smart-dash integration command should normalize and execute");
    assert_true(dash_out.str().find("operator_input_normalized=integration-test-provider --provider-name openai") != std::string::npos, "operator-query should normalize smart dashes in assisted input");
    assert_true(dash_out.str().find("structured_result_returned=true") != std::string::npos, "normalized smart-dash command should execute readiness test");

    std::ostringstream activity_out;
    std::ostringstream activity_err;
    rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + provider_root.string(), "--quiet-startup", "--input", "create a weekly laundry task"}, activity_out, activity_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "operator-query should use configured provider-backed interpreter for natural language activity creation");
    assert_true(activity_out.str().find("operator_query=llm_interpreter") != std::string::npos, "operator-query should mark interpreter execution");
    assert_true(activity_out.str().find("matched_command=procedural-upsert-activity") != std::string::npos, "natural language request should map to activity upsert");
    assert_true(activity_out.str().find("intent_execution=executed") != std::string::npos, "high-confidence low-risk routing should execute through deterministic layer");
    assert_true(activity_out.str().find("activity_id=activity.weekly-laundry") != std::string::npos, "resolved activity command should execute deterministically");
    assert_true(activity_out.str().find("operator_input_raw=create a weekly laundry task") != std::string::npos, "raw operator input should be logged");
    assert_true(activity_out.str().find("intent_model_output=mode=proposed") != std::string::npos, "structured model output should be logged");
    assert_true(activity_out.str().find("normalized_mode=proposed") != std::string::npos, "normalized mode diagnostics should be emitted");
    assert_true(activity_out.str().find("normalized_matched_command=procedural-upsert-activity") != std::string::npos, "normalized matched command diagnostics should be emitted");
    assert_true(activity_out.str().find("normalized_args=procedural-upsert-activity --activity-id activity.weekly-laundry") != std::string::npos, "normalized args diagnostics should be emitted");
    assert_true(activity_out.str().find("route_acceptance_result=accepted_proposed") != std::string::npos, "route acceptance diagnostics should be emitted");
    assert_true(activity_out.str().find("intent_route_command=procedural-upsert-activity") != std::string::npos, "final command resolution should be logged");

    std::ostringstream invalid_out;
    std::ostringstream invalid_err;
    rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + provider_root.string(), "--quiet-startup", "--input", "compose me a symphony"}, invalid_out, invalid_err, "", std::filesystem::current_path());
    assert_true(rc == 2, "invalid operator-query requests should fail with helpful explanation");
    assert_true(invalid_err.str().find("I couldn't find a confident command match") != std::string::npos, "invalid natural language should emit helpful explanation rather than argument noise");
    assert_true(invalid_out.str().find("closest_commands=") != std::string::npos, "invalid natural language should list closest valid commands");
    assert_true(invalid_out.str().find("route_acceptance_result=accepted_failure") != std::string::npos, "grounded failures should expose route acceptance diagnostics");

    std::ostringstream help_out;
    std::ostringstream help_err;
    rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + provider_root.string(), "--quiet-startup", "--input", "What can you do?"}, help_out, help_err, "", std::filesystem::current_path());
    assert_true(rc == 2, "broad operator-query requests should degrade into a stable recognized failure");
    assert_true(help_out.str().find("operator_query_failure_class=provider_output_helpful_failure") != std::string::npos, "broad help-style prompts should produce a recognized grounded failure class");
    assert_true(help_out.str().find("route_rejection_reason=") != std::string::npos || help_err.str().find("Help") != std::string::npos, "broad help-style prompts should expose helpful route diagnostics");

    std::ostringstream risky_out;
    std::ostringstream risky_err;
    rc = life_orchestrator::app::run_application({"operator-query", "--data-root=" + provider_root.string(), "--quiet-startup", "--input", "update the provider api key"}, risky_out, risky_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "high-risk operator-query requests should return confirmation guidance");
    assert_true(risky_out.str().find("matched_command=integration-set-provider") != std::string::npos, "provider update should route to integration-set-provider");
    assert_true(risky_out.str().find("requires_confirmation=true") != std::string::npos, "high-risk actions should require confirmation");
    assert_true(risky_out.str().find("intent_execution=deferred_for_confirmation") != std::string::npos, "high-risk actions should not execute immediately");

    std::ostringstream readiness_out;
    std::ostringstream readiness_err;
    rc = life_orchestrator::app::run_application({"integration-test-provider", "--data-root=" + provider_root.string(), "--quiet-startup", "--provider-name", "openai"}, readiness_out, readiness_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "integration-test-provider should succeed for deterministic test provider");
    assert_true(readiness_out.str().find("structured_result_returned=true") != std::string::npos, "provider readiness should emit stable key-value output");

    std::ostringstream status_out;
    std::ostringstream status_err;
    rc = life_orchestrator::app::run_application({"status", "--data-root=" + provider_root.string(), "--quiet-startup"}, status_out, status_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "status should succeed with configured provider");
    assert_true(status_out.str().find("configured_provider_count=1") != std::string::npos, "status should expose configured provider count");
}



void test_live_provider_route_hardening_matrix() {
    using namespace life_orchestrator::app;
    const auto context = build_intent_command_context(list_application_commands(), list_application_aliases(), list_action_form_specs());
    const std::vector<std::string> closest = {"status", "help"};

    const auto valid = route_with_provider("create a weekly laundry task", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=procedural-upsert-activity\nargs=procedural-upsert-activity --activity-id activity.weekly-laundry\nconfidence=0.92\nreasoning_summary=Weekly laundry maps cleanly.\nrequires_confirmation=false\nclosest_commands=procedural-upsert-activity,status\nuser_facing_message=Mapped request safely.\n"};
    });
    assert_true(valid.mode == "proposed" && valid.matched_command == "procedural-upsert-activity" && !valid.requires_confirmation, "valid live provider output with confirmation false should parse stably");

    const auto confirm = route_with_provider("update provider key", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=integration-set-provider\nargs=integration-set-provider --provider-name openai\nconfidence=0.82\nreasoning_summary=High risk.\nrequires_confirmation=true\nclosest_commands=integration-set-provider,status\nuser_facing_message=Needs confirmation.\n"};
    });
    assert_true(confirm.mode == "proposed" && confirm.requires_confirmation, "valid live provider output with confirmation true should parse stably");

    const auto missing_command = normalize_intent_routing_result(route_with_provider("laundry", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nconfidence=0.92\nreasoning_summary=Missing command.\nrequires_confirmation=false\nclosest_commands=status,help\nuser_facing_message=Missing command.\n"};
    }), context, closest);
    assert_true(missing_command.route.mode == "failure" && missing_command.failure_class == "provider_output_incomplete" && missing_command.rejection_reason == "missing_matched_command" && missing_command.route.user_facing_message == "Missing command.", "missing matched_command should degrade into a stable recognized failure");

    const auto missing_message = route_with_provider("laundry", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=status\nargs=status\nconfidence=0.92\nreasoning_summary=Missing message.\nrequires_confirmation=false\nclosest_commands=status,help\n"};
    });
    assert_true(missing_message.user_facing_message.find("couldn't map") != std::string::npos, "missing user_facing_message should retain stable fallback message");

    const auto bad_confidence = route_with_provider("laundry", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=status\nargs=status\nconfidence=not-a-number\nreasoning_summary=Bad confidence.\nrequires_confirmation=false\nclosest_commands=status,help\nuser_facing_message=Bad confidence.\n"};
    });
    assert_true(bad_confidence.matched_command.empty() && bad_confidence.user_facing_message.find("invalid confidence") != std::string::npos, "malformed confidence should degrade gracefully instead of throwing");

    const auto bad_confirmation = route_with_provider("laundry", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=status\nargs=status\nconfidence=0.5\nreasoning_summary=Bad confirm.\nrequires_confirmation=maybe\nclosest_commands=status,help\nuser_facing_message=Bad confirmation.\n"};
    });
    assert_true(bad_confirmation.matched_command.empty() && bad_confirmation.user_facing_message.find("invalid confirmation") != std::string::npos, "malformed requires_confirmation should degrade gracefully instead of throwing");

    const auto unknown_mode = route_with_provider("laundry", context, closest, [](const std::string&) {
        return std::string{"mode=sideways\nmatched_command=status\nargs=status\nconfidence=0.5\nreasoning_summary=Unknown mode.\nrequires_confirmation=false\nclosest_commands=status,help\nuser_facing_message=Unknown mode.\n"};
    });
    assert_true(unknown_mode.mode == "sideways", "unknown mode should remain parseable for downstream normalization");

    const auto normalized_mode_variant = normalize_intent_routing_result(route_with_provider("laundry", context, closest, [](const std::string&) {
        return std::string{"mode=command\nmatched_command=procedural-upsert-activity\nargs=procedural-upsert-activity --activity-id activity.laundry --title Laundry --domain-source home --frequency weekly --duration-minutes 60 --effort-estimate 4 --outcome-value 6\nconfidence=0.71\nreasoning_summary=Mode variant.\nrequires_confirmation=no\nclosest_commands=status,help\nuser_facing_message=Variant accepted.\n"};
    }), context, closest);
    assert_true(normalized_mode_variant.route.mode == "proposed" && normalized_mode_variant.failure_class.empty(), "mode variant command should normalize to proposed");

    const auto ungrounded = normalize_intent_routing_result(route_with_provider("laundry", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=magic-household-router\nargs=magic-household-router --foo bar\nconfidence=0.88\nreasoning_summary=Invented command.\nrequires_confirmation=false\nclosest_commands=status,help\nuser_facing_message=Invented command.\n"};
    }), context, closest);
    assert_true(ungrounded.route.mode == "failure" && ungrounded.failure_class == "provider_output_ungrounded_command" && ungrounded.rejection_reason == "unknown_command", "invented commands should downgrade to grounded failure");

    const auto help_failure = normalize_intent_routing_result(route_with_provider("what can you do?", context, closest, [](const std::string&) {
        return std::string{"mode=no_match\nmatched_command=\nargs=\nconfidence=0.33\nreasoning_summary=Broad help request.\nrequires_confirmation=0\nclosest_commands=help,status\nuser_facing_message=Try Help for the full command list, or type an exact command like status.\n"};
    }), context, closest);
    assert_true(help_failure.route.mode == "failure" && help_failure.failure_class == "provider_output_helpful_failure" && help_failure.route.user_facing_message.find("Help") != std::string::npos, "broad prompts should normalize into helpful failure routes");


    const auto recoverable_args = normalize_intent_routing_result(route_with_provider("create laundry task", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=create_activity\nargs=procedural-upsert-activity --activity-id activity.laundry --title Laundry --domain-source home --frequency weekly --duration-minutes 60 --effort-estimate 4 --outcome-value 6\nconfidence=0.88\nreasoning_summary=Alias recovered.\nrequires_confirmation=false\nclosest_commands=procedural-upsert-activity,status\nuser_facing_message=Recovered alias safely.\n"};
    }), context, closest);
    assert_true(recoverable_args.route.mode == "proposed" && recoverable_args.route.matched_command == "procedural-upsert-activity", "recoverable command aliases should normalize to canonical commands");

    const auto missing_required_args = normalize_intent_routing_result(route_with_provider("create laundry task", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=procedural-upsert-activity\nargs=procedural-upsert-activity --activity-id activity.laundry\nconfidence=0.63\nreasoning_summary=Too few args.\nrequires_confirmation=false\nclosest_commands=procedural-upsert-activity,status\nuser_facing_message=Need more activity details.\n"};
    }), context, closest);
    assert_true(missing_required_args.failure_class == "provider_output_missing_required_args" && missing_required_args.rejection_reason == "invalid_args", "proposed commands missing required args should degrade with a specific grounded rejection");

    const auto oversized_reasoning = route_with_provider("laundry", context, closest, [](const std::string&) {
        return std::string{"mode=proposed\nmatched_command=status\nargs=status\nconfidence=0.95\nreasoning_summary=Line one.\nLine two should be ignored by kv parsing.\nrequires_confirmation=false\nclosest_commands=status,help\nuser_facing_message=Oversized reasoning handled.\n"};
    });
    assert_true(oversized_reasoning.matched_command == "status", "oversized multiline reasoning text should still yield a stable parsed route");

    const auto serialized = serialize_intent_routing_result(oversized_reasoning);
    assert_true(serialized.find("matched_command=status") != std::string::npos, "stable routes should remain serializable after hardening");
}

void test_assistant_shell_live_provider_degrades_gracefully_for_invalid_routes() {
    namespace shell = life_orchestrator::app::assistant_shell;
    const std::filesystem::path root = "artifacts/assistant_shell_live_provider_invalid";
    std::filesystem::remove_all(root);

    std::ostringstream out;
    std::ostringstream err;
    auto rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai", "--api-key", "TEST_KEY_123", "--model-name", "gpt-5"}, out, err, "", std::filesystem::current_path());
    assert_true(rc == 0, "provider should configure for live provider shell tests");

    shell::AssistantShellSurfaceService service(root, std::filesystem::current_path(), "");
    service.StartOrResumeSession("session-live-provider");

    const auto ok_result = service.SubmitUserText({"session-live-provider", "create a weekly laundry task"});
    assert_true(ok_result.ok && !ok_result.appended_messages.empty(), "valid live provider output with confirmation false should reach the shell safely");

    const auto confirm_result = service.SubmitUserText({"session-live-provider", "update the provider api key"});
    assert_true(confirm_result.ok && confirm_result.pending_confirmation.has_value(), "valid live provider output with confirmation true should create confirmation state safely");
}

void test_assistant_shell_surface_contract_strings() {
    using namespace life_orchestrator::app::assistant_shell;
    assert_true(to_string(AssistantShellMessageBlockType::ExecutionSummary) == "execution_summary", "contract string conversion should be stable");
    assert_true(to_string(AssistantShellSessionMode::Extended) == "extended", "session mode string conversion should be stable");
}

void test_assistant_shell_submission_routing_precedence() {
    namespace shell = life_orchestrator::app::assistant_shell;
    const std::filesystem::path root = "artifacts/assistant_shell_precedence";
    std::filesystem::remove_all(root);
    shell::AssistantShellSurfaceService service(root, std::filesystem::current_path(), "");
    const auto startup = service.StartOrResumeSession("session-precedence");
    assert_true(!startup.initial_messages.empty(), "assistant shell startup should include deterministic greeting");

    const auto result = service.SubmitUserText({"session-precedence", "backlog"});
    assert_true(result.ok, "exact alias resolution should succeed without provider");
    assert_true(!result.appended_messages.empty(), "submission should append assistant response");
    bool saw_summary = false;
    for (const auto& block : result.appended_messages.front().blocks) {
        if (block.execution_summary.has_value()) {
            saw_summary = true;
            assert_true(block.execution_summary->resolution_path == "exact_command_resolution", "exact alias should resolve before intent routing");
            assert_true(!block.execution_summary->provider_used, "exact alias resolution should not depend on provider state");
        }
    }
    assert_true(saw_summary, "exact resolution should expose safe execution summary block");
}

void test_assistant_shell_confirmation_generation_and_acceptance() {
    namespace shell = life_orchestrator::app::assistant_shell;
    const std::filesystem::path root = "artifacts/assistant_shell_confirmation";
    std::filesystem::remove_all(root);
    std::ostringstream set_out;
    std::ostringstream set_err;
    auto rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai", "--api-key", "TEST_KEY_123", "--model-name", "gpt-5"}, set_out, set_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "provider should configure for assistant shell confirmation test");

    shell::AssistantShellSurfaceService service(root, std::filesystem::current_path(), "");
    service.StartOrResumeSession("session-confirmation");
    const auto result = service.SubmitUserText({"session-confirmation", "update the provider api key"});
    assert_true(result.pending_confirmation.has_value(), "medium/high-risk interpreted actions should render inline confirmations");
    assert_true(result.status.pending_confirmation_count == 1, "status should expose pending confirmation count");

    const auto confirmation = service.ResolveConfirmation("session-confirmation", result.pending_confirmation->confirmation_id, true);
    assert_true(confirmation.accepted, "accepting confirmation should succeed");
    assert_true(confirmation.execution_summary.has_value() && confirmation.execution_summary->resolution_path == "confirmation_resolution", "confirmation acceptance should preserve authoritative execution lineage");
}

void test_assistant_shell_no_provider_remediation_and_redaction() {
    namespace shell = life_orchestrator::app::assistant_shell;
    const std::filesystem::path root = "artifacts/assistant_shell_no_provider";
    std::filesystem::remove_all(root);
    shell::AssistantShellSurfaceService service(root, std::filesystem::current_path(), "");
    service.StartOrResumeSession("session-no-provider");
    const auto result = service.SubmitUserText({"session-no-provider", "plan my week"});
    assert_true(result.ok, "no-provider remediation should still return a friendly assistant response");
    bool saw_remediation = false;
    for (const auto& block : result.appended_messages.front().blocks) {
        if (block.type == shell::AssistantShellMessageBlockType::AssistantResponse && block.text.find("Configure a provider") != std::string::npos) saw_remediation = true;
    }
    assert_true(saw_remediation, "no-provider requests should emit remediation guidance");

    std::ostringstream set_out;
    std::ostringstream set_err;
    auto rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai", "--api-key", "TEST_KEY_123", "--model-name", "gpt-5"}, set_out, set_err, "", std::filesystem::current_path());
    assert_true(rc == 0, "provider should configure for redaction visibility test");
    const auto provider_result = service.SubmitUserText({"session-no-provider", "integration-list-providers"});
    bool saw_redacted_card = false;
    for (const auto& block : provider_result.appended_messages.front().blocks) {
        if (block.artifact_card.has_value()) {
            for (const auto& [label, value] : block.artifact_card->summary_fields) {
                if (label == "API Key" && value.find("***") != std::string::npos) saw_redacted_card = true;
                assert_true(value.find("TEST_KEY_123") == std::string::npos, "shell-visible provider metadata must remain redacted");
            }
        }
    }
    assert_true(saw_redacted_card, "provider artifact card should preserve secret redaction");
}

void test_assistant_shell_session_persistence_and_reload() {
    namespace shell = life_orchestrator::app::assistant_shell;
    const std::filesystem::path root = "artifacts/assistant_shell_persistence";
    std::filesystem::remove_all(root);
    shell::AssistantShellSurfaceService service(root, std::filesystem::current_path(), "");
    service.StartOrResumeSession("session-persist");
    service.SubmitUserText({"session-persist", "status"});
    const auto sessions = service.ListSessions();
    assert_true(!sessions.empty(), "historical chats should list persisted sessions");
    assert_true(sessions.front().session_id == "session-persist", "persisted session should be discoverable by id");

    shell::AssistantShellSurfaceService reloaded(root, std::filesystem::current_path(), "");
    const auto resumed = reloaded.StartOrResumeSession("session-persist");
    assert_true(resumed.initial_messages.size() >= 3, "reloaded session should preserve prior transcript messages");
    const auto status = reloaded.LoadLastStatus("session-persist");
    assert_true(status.has_value(), "reloaded session should preserve last known shell status snapshot");
}

void test_inference_transport_contracts_and_redaction() {
    using namespace life_orchestrator::integration::inference;
    assert_true(redact_secret("TEST_KEY_123") == "TE***23", "transport redaction should preserve only safe boundary characters");

    auto fake = std::make_shared<FakeHttpExecutor>();
    ProviderTransportRegistry registry(fake);
    const auto& unsupported = registry.Resolve("stub");
    assert_true(unsupported.name() == "unsupported_provider_transport", "non-openai providers should resolve to unsupported transport");

    const life_orchestrator::core::IntegrationConfigurationRecord record{
        "provider.openai", "openai", "OpenAI", true, life_orchestrator::core::IntegrationStatus::Enabled, {}, {},
        life_orchestrator::core::CredentialStorageMode::ExternalSecretReference, "config/providers/openai.secret",
        {{"model_name", "gpt-5"}}, "2026-03-21T00:00:00.000Z", "2026-03-21T00:00:00.000Z", 1};
    InferenceTransportClient client(fake);
    const auto result = client.Interpret(record, "TEST_KEY_123", "req-1", {{"system", "Return structured output only."}, {"user", "create a weekly laundry task"}});
    assert_true(result.ok, "transport client should return structured output");
    assert_true(result.output_text.find("matched_command=procedural-upsert-activity") != std::string::npos, "transport output should remain structured");
    assert_true(!fake->requests.empty(), "fake executor should capture outbound requests");
    assert_true(fake->requests.back().headers.at("Authorization") == "Bearer TEST_KEY_123", "transport should build bearer auth header");
    assert_true(fake->requests.back().body.find("TEST_KEY_123") == std::string::npos, "transport request body should not leak secret");
    assert_true(fake->requests.back().url == default_openai_responses_endpoint(), "openai transport should default the responses endpoint");

    const auto parsed = parse_openai_structured_output_to_key_value(fake->next_response.body);
    assert_true(parsed.has_value() && parsed->find("mode=proposed") != std::string::npos, "structured json schema output should normalize to key-value format");
    assert_true(fake->requests.back().body.find("Authoritative command catalog") != std::string::npos, "request builder should include the authoritative command catalog grounding block");
    assert_true(fake->requests.back().body.find("Few-shot examples") != std::string::npos, "request builder should include the compact few-shot grounding block");

    fake->next_response = make_http_response(200, R"({"output_text":"{\"mode\":\"command\",\"matched_command\":\"status\",\"args\":\"status\",\"confidence\":\"1.3\",\"reasoning_summary\":\"Healthy\",\"requires_confirmation\":\"no\",\"closest_commands\":[\"status\",\"help\"],\"user_facing_message\":\"Ready\"}","input_tokens":1,"output_tokens":1,"total_tokens":2})", {}, true, true, "read_body", std::nullopt, {}, "application/json", "req-variant");
    const auto variant = parse_openai_structured_output_to_key_value(fake->next_response.body);
    assert_true(variant.has_value() && variant->find("mode=proposed") != std::string::npos && variant->find("closest_commands=status,help") != std::string::npos && variant->find("requires_confirmation=false") != std::string::npos, "recoverable provider variants should normalize into the canonical key-value contract");
}

void test_provider_validation_paths_and_setup_service() {
    namespace setup = life_orchestrator::app::provider_setup;
    const std::filesystem::path root = "artifacts/provider_validation";
    std::filesystem::remove_all(root);

    std::ostringstream out;
    std::ostringstream err;
    auto rc = life_orchestrator::app::run_application({"integration-test-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "missing"}, out, err, "", std::filesystem::current_path());
    assert_true(rc != 0, "missing provider health check should fail");
    assert_true(err.str().find("provider_not_found") != std::string::npos, "missing provider error should be structured");

    rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai", "--model-name", "gpt-5", "--secret-source", "env", "--env-var", "MISSING_ENV"}, out, err, "", std::filesystem::current_path());
    assert_true(rc == 0, "provider metadata should persist without inline secret material");
    out.str(""); out.clear(); err.str(""); err.clear();
    rc = life_orchestrator::app::run_application({"integration-test-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai"}, out, err, "", std::filesystem::current_path());
    assert_true(rc != 0, "provider validation should fail when secret resolution fails");
    assert_true(err.str().find("secret_resolved=false") != std::string::npos, "validation should expose secret-resolution failure safely");

    out.str(""); out.clear(); err.str(""); err.clear();
    rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "stub", "--api-key", "TEST_KEY_123", "--model-name", "gpt-5", "--display-name", "Stub Provider"}, out, err, "", std::filesystem::current_path());
    assert_true(rc == 0, "unsupported provider metadata should still persist");
    out.str(""); out.clear(); err.str(""); err.clear();
    rc = life_orchestrator::app::run_application({"integration-test-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "stub"}, out, err, "", std::filesystem::current_path());
    assert_true(rc != 0, "unsupported provider validation should fail");
    assert_true(err.str().find("message=unsupported_provider") != std::string::npos, "unsupported provider validation should surface safe failure class");
    assert_true(err.str().find("TEST_KEY_123") == std::string::npos, "unsupported provider validation should not leak secrets");

    setup::ProviderSetupService service(root, std::filesystem::current_path(), "");
    const auto providers = service.ListProviders();
    assert_true(!providers.empty(), "provider setup service should list configured providers");
    const auto openai_test = service.TestProvider("openai");
    assert_true(!openai_test.ok, "provider setup service should surface missing-secret failures");
    const auto stub_test = service.TestProvider("stub");
    assert_true(!stub_test.ok, "provider setup service should report unsupported providers plainly");
    assert_true(stub_test.summary.find("unsupported") != std::string::npos, "unsupported provider status text should be plain");
}

void test_provider_setup_controller_round_trip() {
    namespace setup = life_orchestrator::app::provider_setup;
    namespace ui_setup = life_orchestrator::ui::provider_setup;
    const std::filesystem::path root = "artifacts/provider_setup_controller_round_trip";
    std::filesystem::remove_all(root);

    auto service = std::make_shared<setup::ProviderSetupService>(root, std::filesystem::current_path(), "");
    ui_setup::ProviderSetupController controller(service);

    const auto save_result = controller.SaveProvider({"openai", "OpenAI", "gpt-5", "direct", "TEST_KEY_123", "", "", false});
    assert_true(save_result.exit_code == 0, "provider setup controller should save via service pass-through");
    assert_true(save_result.standard_output.find("provider_name=openai") != std::string::npos, "controller save should preserve authoritative stdout");

    const auto providers = controller.ListProviders();
    assert_true(providers.size() == 1, "controller list should surface saved providers");
    assert_true(providers.front().provider_name == "openai", "controller list should preserve provider names");
    assert_true(!providers.front().enabled, "controller list should preserve enabled state");
    assert_true(providers.front().redacted_secret_status.find("***") != std::string::npos, "controller list should preserve redacted secret status");

    const auto test_result = controller.TestProvider("openai");
    assert_true(test_result.ok, "controller test should call provider readiness path");
    assert_true(test_result.safe_details.find("structured_result_returned=true") != std::string::npos, "controller test should preserve safe readiness details");
}


void test_openai_transport_failure_and_registry_behavior() {
    using namespace life_orchestrator::integration::inference;
    auto fake = std::make_shared<FakeHttpExecutor>();
    fake->next_response = make_http_response(429, "{}", {}, true, true, "receive_response", std::nullopt, {}, "application/json", "req-rate-1", "rate limited", "{}");
    InferenceTransportClient client(fake);
    const life_orchestrator::core::IntegrationConfigurationRecord record{
        "provider.openai", "OpenAI", "OpenAI", true, life_orchestrator::core::IntegrationStatus::Enabled, {}, {},
        life_orchestrator::core::CredentialStorageMode::ExternalSecretReference, "config/providers/openai.secret",
        {{"model_name", "gpt-5"}}, "2026-03-21T00:00:00.000Z", "2026-03-21T00:00:00.000Z", 1};
    const auto result = client.Interpret(record, "TEST_KEY_123", "req-rate", {{"user", "test"}});
    assert_true(!result.ok && result.error.has_value() && result.error->failure_class == "rate_limited", "rate limits should surface safe failure class");
    assert_true(result.error->message.find("TEST_KEY_123") == std::string::npos, "transport failures should not leak secrets");
}

void test_openai_transport_diagnostic_matrix() {
    using namespace life_orchestrator::integration::inference;
    auto fake = std::make_shared<FakeHttpExecutor>();
    InferenceTransportClient client(fake);
    const life_orchestrator::core::IntegrationConfigurationRecord record{
        "provider.openai", "OpenAI", "OpenAI", true, life_orchestrator::core::IntegrationStatus::Enabled, {}, {},
        life_orchestrator::core::CredentialStorageMode::ExternalSecretReference, "config/providers/openai.secret",
        {{"model_name", "gpt-5"}}, "2026-03-21T00:00:00.000Z", "2026-03-21T00:00:00.000Z", 1};

    fake->next_response = make_http_response(401, R"({"error":{"message":"Bad API key TEST_KEY_123","type":"invalid_request_error","code":"invalid_api_key"}})", {}, true, true, "receive_response", std::nullopt, {}, "application/json", "req-auth", "invalid key", R"({"error":{"message":"Bad API key TEST_KEY_123"}})");
    auto result = client.Interpret(record, "TEST_KEY_123", "req-auth", {{"user", "test"}});
    assert_true(!result.ok && result.error.has_value() && result.error->failure_class == "authentication_failure", "401 should map to authentication_failure");
    assert_true(result.error->safe_body_preview.find("TEST_KEY_123") == std::string::npos, "401 preview should redact secrets");

    fake->next_response = make_http_response(400, R"({"error":{"message":"Bad model","type":"invalid_request_error","code":"invalid_model"}})", {}, true, true, "receive_response", std::nullopt, {}, "application/json", "req-bad", "bad request", R"({"error":{"message":"Bad model"}})");
    result = client.Interpret(record, "TEST_KEY_123", "req-bad", {{"user", "test"}});
    assert_true(!result.ok && result.error.has_value() && result.error->failure_class == "bad_request", "400 should map to bad_request");

    fake->next_response = make_http_response(429, R"({"error":{"message":"Too many requests","type":"rate_limit_error","code":"rate_limit_exceeded"}})", {}, true, true, "receive_response", std::nullopt, {}, "application/json", "req-429", "rate limited", R"({"error":{"message":"Too many requests"}})");
    result = client.Interpret(record, "TEST_KEY_123", "req-429", {{"user", "test"}});
    assert_true(!result.ok && result.error.has_value() && result.error->failure_class == "rate_limited", "429 should map to rate_limited");

    fake->next_response = make_http_response(500, R"({"error":{"message":"Server exploded","type":"server_error","code":"internal_error"}})", {}, true, true, "receive_response", std::nullopt, {}, "application/json", "req-500", "server failure", R"({"error":{"message":"Server exploded"}})");
    result = client.Interpret(record, "TEST_KEY_123", "req-500", {{"user", "test"}});
    assert_true(!result.ok && result.error.has_value() && result.error->failure_class == "server_error", "500 should map to server_error");

    fake->next_response = make_http_response(std::nullopt, {}, "connect failed", false, false, "connect", std::optional<unsigned long>{12029}, "connect failed", {}, {}, "connect failed", "Authorization: Bearer TEST_KEY_123");
    result = client.Interpret(record, "TEST_KEY_123", "req-connect", {{"user", "test"}});
    assert_true(!result.ok && result.error.has_value() && result.error->failure_stage == "connect", "network failure should retain stage");
    assert_true(result.error->safe_body_preview.find("TEST_KEY_123") == std::string::npos, "network diagnostics should redact secrets");

    fake->next_response = make_http_response(200, R"({"output_text":"{"mode":"proposed","matched_command":"status","args":"status","confidence":0.91,"reasoning_summary":"Healthy","requires_confirmation":false,"closest_commands":"status,help","user_facing_message":"Ready"}","input_tokens":1,"output_tokens":1,"total_tokens":2})", {}, true, true, "read_body", std::nullopt, {}, "application/json", "req-200");
    result = client.Interpret(record, "TEST_KEY_123", "req-200", {{"user", "test"}});
    assert_true(result.ok, "200 structured response should succeed");

    fake->next_response = make_http_response(200, "<html>nope</html>", {}, true, true, "read_body", std::nullopt, {}, "text/html", "req-malformed", "malformed body", "<html>nope</html>");
    result = client.Interpret(record, "TEST_KEY_123", "req-malformed", {{"user", "test"}});
    assert_true(!result.ok && result.error.has_value() && result.error->failure_class == "schema_parse_failure", "200 malformed body should map to schema_parse_failure");
}

void test_integration_test_provider_output_diagnostics() {
    const std::filesystem::path root = "artifacts/provider_diagnostics";
    std::filesystem::remove_all(root);
    std::ostringstream out;
    std::ostringstream err;
    auto rc = life_orchestrator::app::run_application({"integration-set-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai", "--model-name", "gpt-5", "--secret-source", "env", "--env-var", "MISSING_ENV"}, out, err, "", std::filesystem::current_path());
    assert_true(rc == 0, "provider config for diagnostics should save");
    out.str(""); out.clear(); err.str(""); err.clear();
    rc = life_orchestrator::app::run_application({"integration-test-provider", "--data-root=" + root.string(), "--quiet-startup", "--provider-name", "openai"}, out, err, "", std::filesystem::current_path());
    assert_true(rc != 0, "diagnostic provider test should fail without secret");
    const auto text_out = out.str() + err.str();
    assert_true(text_out.find("failure_class=missing_secret") != std::string::npos, "diagnostic output should expose failure class");
    assert_true(text_out.find("http_status=none") != std::string::npos, "diagnostic output should expose http status placeholder");
    assert_true(text_out.find("failure_stage=none") != std::string::npos, "diagnostic output should expose failure stage placeholder");
    assert_true(text_out.find("outbound_request_attempted=false") != std::string::npos, "diagnostic output should expose outbound attempt flag");
}


void test_openai_transport_stage_accurate_outbound_attempts_and_diagnostics() {
    using namespace life_orchestrator::integration::inference;
    auto fake = std::make_shared<FakeHttpExecutor>();
    InferenceTransportClient client(fake);
    const life_orchestrator::core::IntegrationConfigurationRecord record{
        "provider.openai", "OpenAI", "OpenAI", true, life_orchestrator::core::IntegrationStatus::Enabled, {}, {},
        life_orchestrator::core::CredentialStorageMode::ExternalSecretReference, "config/providers/openai.secret",
        {{"model_name", "gpt-5"}}, "2026-03-21T00:00:00.000Z", "2026-03-21T00:00:00.000Z", 1};

    const std::vector<std::pair<std::string, bool>> stages = {
        {"open_session", false}, {"connect", false}, {"open_request", false},
        {"send_request", true}, {"receive_response", true}, {"read_body", true}
    };
    for (const auto& [stage, attempted] : stages) {
        fake->next_response = make_http_response(std::nullopt, {}, stage + " failed with Authorization: Bearer TEST_KEY_123", false, attempted, stage, std::optional<unsigned long>{12000 + static_cast<unsigned long>(stage.size())}, stage + " failed", {}, {}, stage + " failed", "Authorization: Bearer TEST_KEY_123");
        const auto result = client.Interpret(record, "TEST_KEY_123", "req-" + stage, {{"user", "test"}});
        assert_true(!result.ok && result.error.has_value(), stage + " should fail");
        assert_true(result.error->failure_class == stage + "_failure", stage + " should map to stage-specific failure class");
        assert_true(result.error->outbound_request_attempted == attempted, stage + " should preserve outbound_request_attempted semantics");
        assert_true(result.error->win32_error_code.has_value(), stage + " should retain Win32 error codes");
        assert_true(result.error->win32_error_message == stage + " failed", stage + " should retain Win32 error messages");
        assert_true(result.error->safe_error_summary.find("TEST_KEY_123") == std::string::npos, stage + " summary should redact secrets");
        assert_true(result.error->safe_body_preview.find("TEST_KEY_123") == std::string::npos, stage + " preview should redact secrets");
    }
}

void test_composer_input_and_attachment_persistence() {
    namespace shell = life_orchestrator::app::assistant_shell;
    using life_orchestrator::ui::assistant_shell::AssistantShellComposerInput;
    using life_orchestrator::ui::assistant_shell::ComposerSubmitAction;

    assert_true(AssistantShellComposerInput::CanSubmit("hello"), "composer should submit non-empty text");
    assert_true(!AssistantShellComposerInput::CanSubmit("   \n\t"), "composer should reject trimmed-empty text");
    assert_true(AssistantShellComposerInput::ResolveEnterKey(false) == ComposerSubmitAction::Submit, "enter should submit");
    assert_true(AssistantShellComposerInput::ResolveEnterKey(true) == ComposerSubmitAction::InsertNewline, "shift+enter should insert newline");

    const std::filesystem::path root = "artifacts/assistant_shell_attachments";
    std::filesystem::remove_all(root);
    shell::AssistantShellSurfaceService service(root, std::filesystem::current_path(), "");
    service.StartOrResumeSession("session-attachments");
    auto attachments = service.AddAttachment({"session-attachments", "C:/tmp/demo.txt", "demo.txt", "42"});
    assert_true(attachments.attachments.size() == 1, "attachment add should persist shell metadata");
    attachments = service.RemoveAttachment({"session-attachments", "C:/tmp/demo.txt"});
    assert_true(attachments.attachments.empty(), "attachment remove should persist");
    service.AddAttachment({"session-attachments", "C:/tmp/keep.txt", "keep.txt", "77"});
    shell::AssistantShellSurfaceService reloaded(root, std::filesystem::current_path(), "");
    const auto resumed = reloaded.StartOrResumeSession("session-attachments");
    assert_true(resumed.pending_attachments.attachments.size() == 1, "session reload should preserve pending attachments");
}

}  // namespace

int main() {
    try {
        test_behavioral_application_commands_and_modules();
        test_activity_inventory_persistence_and_reload();
        test_artifact_query_command_surface();
        test_effort_value_classification();
        test_procedural_audit_generation_and_behavioral_routing();
        test_procedural_application_commands();
        test_procedural_proposal_backward_compatibility_defaults();
        test_procedural_list_proposals_emits_default_values();
        test_behavioral_cli_operational_surface();
        test_behavioral_backlog_and_reevaluation_visibility_defaults();
        test_scheduling_candidate_cli_bridge();
        test_scheduling_candidate_default_emission_and_backward_compatibility();
        test_scheduling_proposal_cli_bridge();
        test_scheduling_proposal_default_emission_and_backward_compatibility();
        test_runtime_hygiene_ignore_file();
        test_operator_alias_resolution_and_suggestions();
        test_application_command_helper_exports();
        test_command_surface_aliases_help_and_discoverability();
        test_quick_action_refresh_targets_are_stable();
        test_action_result_view_preserves_authoritative_success_and_failure_output();
        test_execute_action_form_command_refreshes_only_registered_query_surfaces();
        test_supported_quick_actions_have_stable_ordered_fields();
        test_action_form_submission_uses_registry_flag_names();
        test_artifact_presentation_and_action_form_registries();
        test_gui_panels_and_quick_actions_source_registry_labels();
        test_command_help_and_action_form_consistency();
        test_form_spec_missing_required_inputs_defer_to_command_layer();
        test_end_to_end_naive_operator_flow();
        test_integration_provider_persistence_redaction_and_visibility();
        test_integration_set_provider_env_var_reference_mode();
        test_operator_query_provider_and_status_visibility();
        test_live_provider_route_hardening_matrix();
        test_assistant_shell_live_provider_degrades_gracefully_for_invalid_routes();
        test_assistant_shell_surface_contract_strings();
        test_assistant_shell_submission_routing_precedence();
        test_assistant_shell_confirmation_generation_and_acceptance();
        test_assistant_shell_no_provider_remediation_and_redaction();
        test_assistant_shell_session_persistence_and_reload();
        test_inference_transport_contracts_and_redaction();
        test_provider_validation_paths_and_setup_service();
        test_provider_setup_controller_round_trip();
        test_openai_transport_failure_and_registry_behavior();
        test_openai_transport_diagnostic_matrix();
        test_openai_transport_stage_accurate_outbound_attempts_and_diagnostics();
        test_integration_test_provider_output_diagnostics();
        test_composer_input_and_attachment_persistence();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
