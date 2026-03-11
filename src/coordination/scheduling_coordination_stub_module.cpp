#include "coordination/scheduling_coordination_stub_module.hpp"

#include <algorithm>

namespace life_orchestrator::coordination {

SchedulingCoordinationStubModule::SchedulingCoordinationStubModule(core::MemoryService* memory_service)
    : descriptor_{.module_id = "coordination.scheduling_stub",
                  .module_name = "Scheduling Coordination Stub",
                  .module_class = core::ModuleClass::Coordination,
                  .capability_description = "Provides deterministic scheduling placeholder operations.",
                  .capabilities = {"scheduling.health_check"},
                  .input_schema_description = "parameters: optional key-value map",
                  .output_schema_description = "output_data: status and module identity",
                  .state_representation_description = "stateless for sprint step 1",
                  .dependencies = {},
                  .risk_tier = core::RiskTier::Suggestive},
      memory_service_(memory_service) {}

const core::ModuleDescriptor& SchedulingCoordinationStubModule::descriptor() const { return descriptor_; }

bool SchedulingCoordinationStubModule::supports_capability(const core::CapabilityId& capability_id) const {
    return std::find(descriptor_.capabilities.begin(), descriptor_.capabilities.end(), capability_id) !=
           descriptor_.capabilities.end();
}

core::ActionResponse SchedulingCoordinationStubModule::execute(const core::ActionRequest& request) {
    if (memory_service_ != nullptr) {
        memory_service_->log_module_execution_episode(descriptor_.module_id,
                                                      "scheduling.health_check",
                                                      "Scheduling health check executed.",
                                                      {});
    }

    return {.request_id = request.request_id,
            .status = core::ExecutionStatus::Succeeded,
            .responding_module_id = descriptor_.module_id,
            .message = "Scheduling health check passed.",
            .output_data = {{"health", "ok"}, {"capability", request.capability_id}},
            .completed_at = core::current_timestamp_utc()};
}

}  // namespace life_orchestrator::coordination
