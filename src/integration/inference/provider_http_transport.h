#pragma once

#include "integration/inference/http_executor_contracts.h"
#include "integration/inference/inference_transport_contracts.h"

#include <memory>

namespace life_orchestrator::integration::inference {

class ProviderHttpTransport {
public:
    explicit ProviderHttpTransport(std::shared_ptr<IHttpExecutor> executor = make_default_http_executor(), bool supported_provider = true);

    InferenceTransportResult Run(const InferenceTransportRequest& request) const;
    ProviderHealthCheckResult HealthCheck(const ProviderHealthCheckRequest& request) const;
    std::string name() const { return supported_provider_ ? "openai_responses_transport" : "unsupported_provider_transport"; }
private:
    std::shared_ptr<IHttpExecutor> executor_;
    bool supported_provider_ = true;
};

}  // namespace life_orchestrator::integration::inference
