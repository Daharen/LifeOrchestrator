#include "app/app_support/artifact_query_module.hpp"

namespace life_orchestrator::app {

ArtifactQueryModule::ArtifactQueryModule(const ArtifactQueryService* artifact_query_service)
    : artifact_query_service_(artifact_query_service),
      descriptor_{"memory.artifact_query",
                  "Artifact Query Module",
                  core::ModuleClass::CoreInfrastructure,
                  "Routes artifact.query requests directly to persisted artifact memory adapters.",
                  {"artifact.query"},
                  "artifact_type:string, limit?:number",
                  "artifact envelopes",
                  "read-only memory-backed artifact query surface",
                  {},
                  core::RiskTier::Informational} {}

const core::ModuleDescriptor& ArtifactQueryModule::descriptor() const { return descriptor_; }

bool ArtifactQueryModule::supports_capability(const core::CapabilityId& capability_id) const {
    return capability_id == "artifact.query";
}

core::ActionResponse ArtifactQueryModule::execute(const core::ActionRequest& request) {
    if (artifact_query_service_ == nullptr) {
        return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, "Artifact query service unavailable.", {}, core::current_timestamp_utc()};
    }
    const auto artifact_type_it = request.parameters.find("artifact_type");
    if (artifact_type_it == request.parameters.end() || artifact_type_it->second.empty()) {
        return {request.request_id, core::ExecutionStatus::InvalidRequest, descriptor_.module_id, "Missing artifact_type.", {}, core::current_timestamp_utc()};
    }
    std::optional<std::size_t> limit;
    if (const auto limit_it = request.parameters.find("limit"); limit_it != request.parameters.end() && !limit_it->second.empty()) {
        limit = static_cast<std::size_t>(std::stoull(limit_it->second));
    }
    const auto result = artifact_query_service_->query({artifact_type_it->second, limit});
    if (!result.ok || !result.value) {
        return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, result.message, {}, core::current_timestamp_utc()};
    }
    return {request.request_id,
            core::ExecutionStatus::Succeeded,
            descriptor_.module_id,
            "Artifact query completed.",
            {{"artifact_type", artifact_type_it->second}, {"artifact_count", std::to_string(result.value->artifacts.size())}},
            core::current_timestamp_utc()};
}

}  // namespace life_orchestrator::app
