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
std::string join_sorted_pairs(const life_orchestrator::core::ProceduralAttributes& attributes) {
    std::vector<std::pair<std::string, std::string>> ordered(attributes.begin(), attributes.end());
    std::sort(ordered.begin(), ordered.end());
    std::ostringstream out;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        if (i > 0) out << '|';
        out << ordered[i].first << '=' << ordered[i].second;
    }
    return out.str();
}
std::string fingerprint_inventory(const std::vector<life_orchestrator::core::ActivityInventoryItem>& items) {
    std::uint64_t hash = 1469598103934665603ull;
    auto mix = [&](const std::string& value) {
        for (unsigned char ch : value) {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        hash ^= 0xff;
        hash *= 1099511628211ull;
    };
    for (const auto& item : items) {
        mix(item.activity_inventory_item_id);
        mix(item.title);
        mix(item.description);
        mix(item.domain_source);
        mix(item.frequency);
        mix(std::to_string(item.duration_minutes));
        mix(std::to_string(item.effort_estimate));
        mix(std::to_string(item.outcome_value));
        mix(join_sorted_pairs(item.attributes));
    }
    std::ostringstream out;
    out << std::hex << hash;
    return out.str();
}
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
    const auto activity_id = param(request, "activity_inventory_item_id", "activity." + request.request_id);
    auto existing = memory_service_->get_activity_inventory_item_by_id(activity_id);
    core::ProceduralAttributes attributes = existing.ok ? existing.value->attributes : core::ProceduralAttributes{};
    for (const auto& [key, value] : request.parameters) {
        if (key.rfind("attribute.", 0) == 0) attributes[key.substr(10)] = value;
    }
    const auto set_attr = [&](const std::string& key, const std::string& req_key) {
        const auto value = param(request, req_key);
        if (!value.empty()) attributes[key] = value;
    };
    set_attr("repeatable", "repeatable");
    set_attr("necessity", "necessity");
    set_attr("cognitive_load", "cognitive_load");
    set_attr("stress_load", "stress_load");
    set_attr("financial_cost", "financial_cost");
    core::ActivityInventoryItem item{activity_id,
                                     param(request, "title", existing.ok ? existing.value->title : "Untitled Activity"),
                                     param(request, "description", existing.ok ? existing.value->description : ""),
                                     param(request, "domain_source", existing.ok ? existing.value->domain_source : "general"),
                                     param(request, "frequency", existing.ok ? existing.value->frequency : "weekly"),
                                     to_int(param(request, "duration_minutes"), existing.ok ? existing.value->duration_minutes : 30),
                                     to_int(param(request, "effort_estimate"), existing.ok ? existing.value->effort_estimate : 5),
                                     to_int(param(request, "outcome_value"), existing.ok ? existing.value->outcome_value : 5),
                                     descriptor_.module_id,
                                     existing.ok ? existing.value->created_at : now,
                                     now,
                                     existing.ok ? existing.value->version + 1 : 1,
                                     std::move(attributes)};
    const auto result = memory_service_->upsert_activity_inventory_item(item);
    if (!result.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, result.message, {}, core::current_timestamp_utc()};
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Activity inventory item upserted.", {{"activity_inventory_item_id", item.activity_inventory_item_id}, {"version", std::to_string(item.version)}}, core::current_timestamp_utc()};
}

core::ActionResponse ProceduralAuditorModule::audit_inventory(const core::ActionRequest& request) {
    const auto now = param(request, "now", core::current_timestamp_utc());
    auto inventory = memory_service_->list_activity_inventory_items();
    if (!inventory.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, inventory.message, {}, core::current_timestamp_utc()};
    const auto fingerprint = fingerprint_inventory(*inventory.value);
    const auto audit_run_id = param(request, "procedural_audit_run_id", "audit." + fingerprint);
    auto existing_run = memory_service_->get_procedural_audit_run_by_id(audit_run_id);
    auto proposals = engine_.audit(*inventory.value, audit_run_id, descriptor_.module_id, now);
    int triaged_count = 0;
    std::string first_proposal_id;
    for (auto& proposal : proposals) {
        auto existing_proposal = memory_service_->get_optimization_proposal_record_by_id(proposal.optimization_proposal_id);
        if (existing_proposal.ok) {
            proposal.created_at = existing_proposal.value->created_at;
            proposal.version = existing_proposal.value->version + 1;
        }
        const auto behavioral_id = "behavioral." + proposal.optimization_proposal_id;
        const auto triage = control_plane_->dispatch({"triage." + proposal.optimization_proposal_id,
                                                      "behavioral.triage_proposals",
                                                      descriptor_.module_id,
                                                      core::RiskTier::Suggestive,
                                                      {{"proposal_count", "1"},
                                                       {"proposal_id", behavioral_id},
                                                       {"source_proposal_id", proposal.optimization_proposal_id},
                                                       {"source_audit_run_id", proposal.source_audit_run_id},
                                                       {"source_activity_id", proposal.activity_inventory_item_id},
                                                       {"proposal_type", proposal.opportunity_type == core::OptimizationOpportunityType::Automation ? "AutomationAdoption" : "RoutineAdjustment"},
                                                       {"title", proposal.title},
                                                       {"description", proposal.rationale},
                                                       {"priority", proposal.effort_value_classification == core::EffortValueClassification::HighEffortLowValue ? "Critical" : "High"},
                                                       {"estimated_behavioral_effort", proposal.effort_value_classification == core::EffortValueClassification::HighEffortLowValue ? "2" : "3"},
                                                       {"expected_benefit", std::to_string(std::max(1, proposal.energy_recovery_estimate.recovered_effort_points))},
                                                       {"expected_time_cost_minutes", std::to_string(std::max(5, proposal.financial_cost_estimate == 0 ? 15 : 20))},
                                                       {"earliest_presentation_time", now},
                                                       {"decision_time", now},
                                                       {"related_entity_ids", proposal.activity_inventory_item_id}},
                                                      now});
        proposal.linked_behavioral_proposal_id = behavioral_id;
        proposal.triage_status = triage.status == core::ExecutionStatus::Succeeded ? (triage.output_data.at("approved_count") == "1" ? "Approved" : (triage.output_data.at("deferred_count") == "1" ? "Deferred" : (triage.output_data.at("rejected_count") == "1" ? "Rejected" : "Backlogged"))) : "Failed";
        proposal.triage_decision_id = triage.status == core::ExecutionStatus::Succeeded ? "decision." + behavioral_id + "." + now : "";
        proposal.attributes["behavioral_payload_id"] = behavioral_id;
        proposal.attributes["inventory_snapshot_fingerprint"] = fingerprint;
        proposal.attributes["acceptance_status"] = proposal.triage_status;
        proposal.attributes["source_audit_run_id"] = audit_run_id;
        memory_service_->upsert_optimization_proposal_record(proposal);
        ++triaged_count;
        if (first_proposal_id.empty()) first_proposal_id = proposal.optimization_proposal_id;
    }
    memory_service_->upsert_procedural_audit_run_record({audit_run_id,
                                                         descriptor_.module_id,
                                                         existing_run.ok ? existing_run.value->created_at : now,
                                                         now,
                                                         existing_run.ok ? existing_run.value->version + 1 : 1,
                                                         inventory.value->size(),
                                                         proposals.size(),
                                                         "Completed",
                                                         "Procedural audit completed.",
                                                         {{"inventory_snapshot_fingerprint", fingerprint},
                                                          {"triaged_count", std::to_string(triaged_count)},
                                                          {"acceptance_status", triaged_count > 0 ? proposals.front().triage_status : "None"}}});
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Procedural audit completed.", {{"proposal_count", std::to_string(proposals.size())}, {"triaged_count", std::to_string(triaged_count)}, {"first_proposal_id", first_proposal_id}, {"procedural_audit_run_id", audit_run_id}, {"inventory_snapshot_fingerprint", fingerprint}}, core::current_timestamp_utc()};
}

core::ActionResponse ProceduralAuditorModule::list_optimization_proposals(const core::ActionRequest& request) {
    auto proposals = memory_service_->list_optimization_proposal_records();
    if (!proposals.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, proposals.message, {}, core::current_timestamp_utc()};
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Optimization proposals listed.", {{"proposal_count", std::to_string(proposals.value->size())}, {"first_proposal_id", proposals.value->empty() ? std::string{} : proposals.value->front().optimization_proposal_id}}, core::current_timestamp_utc()};
}

core::ActionResponse ProceduralAuditorModule::health_check(const core::ActionRequest& request) {
    const auto now = param(request, "now", "2026-03-18T09:00:00.000Z");
    upsert_activity({request.request_id + ".activity1", "procedural.upsert_activity", request.origin, request.requested_risk_tier, {{"activity_inventory_item_id", "activity.email_triage"}, {"title", "Email triage"}, {"domain_source", "operations"}, {"frequency", "daily"}, {"duration_minutes", "45"}, {"effort_estimate", "8"}, {"outcome_value", "3"}, {"repeatable", "1"}, {"now", now}}, now});
    upsert_activity({request.request_id + ".activity2", "procedural.upsert_activity", request.origin, request.requested_risk_tier, {{"activity_inventory_item_id", "activity.manual_reporting"}, {"title", "Manual reporting"}, {"domain_source", "finance"}, {"frequency", "weekly"}, {"duration_minutes", "60"}, {"effort_estimate", "7"}, {"outcome_value", "4"}, {"financial_cost", "25"}, {"now", now}}, now});
    return audit_inventory({request.request_id + ".audit", "procedural.audit_inventory", request.origin, request.requested_risk_tier, {{"procedural_audit_run_id", "audit.procedural_health_check"}, {"now", now}}, now});
}

}  // namespace life_orchestrator::meta
