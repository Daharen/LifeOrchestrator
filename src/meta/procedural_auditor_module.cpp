#include "meta/procedural_auditor_module.hpp"

#include <algorithm>
#include <sstream>

namespace life_orchestrator::meta {
namespace {
std::string param(const life_orchestrator::core::ActionRequest& request, const std::string& key, const std::string& fallback = "") {
    auto it = request.parameters.find(key);
    return it == request.parameters.end() ? fallback : it->second;
}
int to_int(const std::string& value, int fallback) { return value.empty() ? fallback : std::stoi(value); }
}
namespace core = life_orchestrator::core;

ProceduralAuditorModule::ProceduralAuditorModule(core::MemoryService* memory_service,
                                                 control_plane::ControlPlane* control_plane)
    : descriptor_{"meta.procedural_auditor", "Procedural Auditor Module", core::ModuleClass::Intelligence,
                  "Deterministic structured procedural auditing and proposal generation.",
                  {"procedural.upsert_activity", "procedural.audit_inventory", "procedural.list_optimization_proposals", "procedural.health_check"},
                  "Explicit key-value activity inventory and audit parameters.",
                  "Structured procedural proposal and triage summaries.",
                  "File-backed procedural inventory, audits, and optimization proposals.",
                  {"coordination.behavioral_triage"}, core::RiskTier::Suggestive},
      memory_service_(memory_service),
      control_plane_(control_plane) {}

const core::ModuleDescriptor& ProceduralAuditorModule::descriptor() const { return descriptor_; }
bool ProceduralAuditorModule::supports_capability(const core::CapabilityId& capability_id) const { return std::find(descriptor_.capabilities.begin(), descriptor_.capabilities.end(), capability_id) != descriptor_.capabilities.end(); }

core::ActionResponse ProceduralAuditorModule::execute(const core::ActionRequest& request) {
    memory_service_->log_module_execution_episode(descriptor_.module_id, request.capability_id, "Procedural auditor operation started.", {});
    if (request.capability_id == "procedural.upsert_activity") return upsert_activity(request);
    if (request.capability_id == "procedural.audit_inventory") return audit_inventory(request);
    if (request.capability_id == "procedural.list_optimization_proposals") return list_optimization_proposals(request);
    if (request.capability_id == "procedural.health_check") return health_check(request);
    return {request.request_id, core::ExecutionStatus::NotFound, descriptor_.module_id, "Capability not supported.", {}, core::current_timestamp_utc()};
}

core::ActionResponse ProceduralAuditorModule::upsert_activity(const core::ActionRequest& request) {
    const auto now = param(request, "now", core::current_timestamp_utc());
    core::ActivityInventoryItem item{param(request, "activity_inventory_item_id", "activity." + request.request_id),
                                     param(request, "title", "Untitled Activity"),
                                     param(request, "description"),
                                     param(request, "domain_source", "general"),
                                     param(request, "frequency", "weekly"),
                                     to_int(param(request, "duration_minutes"), 30),
                                     to_int(param(request, "effort_estimate"), 5),
                                     to_int(param(request, "outcome_value"), 5),
                                     descriptor_.module_id,
                                     now,
                                     now,
                                     1,
                                     {{"repeatable", param(request, "repeatable", "0")}}};
    const auto result = memory_service_->upsert_activity_inventory_item(item);
    if (!result.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, result.message, {}, core::current_timestamp_utc()};
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Activity inventory item upserted.", {{"activity_inventory_item_id", item.activity_inventory_item_id}}, core::current_timestamp_utc()};
}

core::ActionResponse ProceduralAuditorModule::audit_inventory(const core::ActionRequest& request) {
    const auto now = param(request, "now", core::current_timestamp_utc());
    const auto audit_run_id = param(request, "procedural_audit_run_id", "audit." + request.request_id);
    auto inventory = memory_service_->list_activity_inventory_items();
    if (!inventory.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, inventory.message, {}, core::current_timestamp_utc()};
    auto proposals = engine_.audit(*inventory.value, audit_run_id, descriptor_.module_id, now);
    int triaged_count = 0;
    std::string first_proposal_id;
    for (auto& proposal : proposals) {
        const auto behavioral_id = "behavioral." + proposal.optimization_proposal_id;
        const auto triage = control_plane_->dispatch({"triage." + proposal.optimization_proposal_id,
                                                      "behavioral.triage_proposals",
                                                      descriptor_.module_id,
                                                      core::RiskTier::Suggestive,
                                                      {{"proposal_count", "1"},
                                                       {"proposal_id", behavioral_id},
                                                       {"proposal_type", proposal.opportunity_type == core::OptimizationOpportunityType::Automation ? "AutomationAdoption" : "RoutineAdjustment"},
                                                       {"title", proposal.title},
                                                       {"description", proposal.rationale},
                                                       {"priority", proposal.effort_value_classification == core::EffortValueClassification::HighEffortLowValue ? "Critical" : "High"},
                                                       {"estimated_behavioral_effort", proposal.effort_value_classification == core::EffortValueClassification::HighEffortLowValue ? "2" : "3"},
                                                       {"expected_benefit", std::to_string(std::max(1, proposal.energy_recovery_estimate.recovered_effort_points))},
                                                       {"earliest_presentation_time", now},
                                                       {"decision_time", now},
                                                       {"related_entity_ids", proposal.activity_inventory_item_id}},
                                                      now});
        proposal.linked_behavioral_proposal_id = behavioral_id;
        proposal.triage_status = triage.status == core::ExecutionStatus::Succeeded ? (triage.output_data.at("approved_count") == "1" ? "Approved" : (triage.output_data.at("deferred_count") == "1" ? "Deferred" : (triage.output_data.at("rejected_count") == "1" ? "Rejected" : "Backlogged"))) : "Failed";
        proposal.triage_decision_id = triage.status == core::ExecutionStatus::Succeeded ? "decision." + behavioral_id + "." + now : "";
        proposal.attributes["behavioral_payload_id"] = behavioral_id;
        memory_service_->upsert_optimization_proposal_record(proposal);
        ++triaged_count;
        if (first_proposal_id.empty()) first_proposal_id = proposal.optimization_proposal_id;
    }
    memory_service_->upsert_procedural_audit_run_record({audit_run_id, descriptor_.module_id, now, now, 1, inventory.value->size(), proposals.size(), "Completed", "Procedural audit completed.", {}});
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Procedural audit completed.", {{"proposal_count", std::to_string(proposals.size())}, {"triaged_count", std::to_string(triaged_count)}, {"first_proposal_id", first_proposal_id}, {"procedural_audit_run_id", audit_run_id}}, core::current_timestamp_utc()};
}

core::ActionResponse ProceduralAuditorModule::list_optimization_proposals(const core::ActionRequest& request) {
    auto proposals = memory_service_->list_optimization_proposal_records();
    if (!proposals.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, proposals.message, {}, core::current_timestamp_utc()};
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Optimization proposals listed.", {{"proposal_count", std::to_string(proposals.value->size())}, {"first_proposal_id", proposals.value->empty() ? std::string{} : proposals.value->front().optimization_proposal_id}}, core::current_timestamp_utc()};
}

core::ActionResponse ProceduralAuditorModule::health_check(const core::ActionRequest& request) {
    const auto now = param(request, "now", "2026-03-18T09:00:00.000Z");
    upsert_activity({request.request_id + ".activity1", "procedural.upsert_activity", request.origin, request.requested_risk_tier, {{"activity_inventory_item_id", "activity.email_triage"}, {"title", "Email triage"}, {"domain_source", "operations"}, {"frequency", "daily"}, {"duration_minutes", "45"}, {"effort_estimate", "8"}, {"outcome_value", "3"}, {"repeatable", "1"}, {"now", now}}, now});
    upsert_activity({request.request_id + ".activity2", "procedural.upsert_activity", request.origin, request.requested_risk_tier, {{"activity_inventory_item_id", "activity.manual_reporting"}, {"title", "Manual reporting"}, {"domain_source", "finance"}, {"frequency", "weekly"}, {"duration_minutes", "60"}, {"effort_estimate", "7"}, {"outcome_value", "4"}, {"now", now}}, now});
    return audit_inventory({request.request_id + ".audit", "procedural.audit_inventory", request.origin, request.requested_risk_tier, {{"procedural_audit_run_id", "audit.procedural_health_check"}, {"now", now}}, now});
}

}  // namespace life_orchestrator::meta
