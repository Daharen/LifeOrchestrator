#pragma once

#include "core/memory.hpp"
#include "integration/inference/http_executor_contracts.h"
#include "integration/inference/inference_transport_contracts.h"
#include "integration/inference/provider_transport_registry.h"

namespace life_orchestrator::integration::inference {

class InferenceTransportClient {
public:
    explicit InferenceTransportClient(ProviderTransportRegistry registry = {});
    explicit InferenceTransportClient(std::shared_ptr<IHttpExecutor> executor);

    InferenceTransportResult Interpret(const core::IntegrationConfigurationRecord& record,
                                       const std::string& api_key,
                                       const std::string& request_id,
                                       const std::vector<InferenceTransportMessage>& messages) const;

    ProviderHealthCheckResult CheckProvider(const core::IntegrationConfigurationRecord& record,
                                            const std::string& api_key,
                                            const std::string& request_id) const;
private:
    InferenceTransportOptions BuildOptions(const core::IntegrationConfigurationRecord& record,
                                           const std::string& api_key) const;

    ProviderTransportRegistry registry_;
};

}  // namespace life_orchestrator::integration::inference
