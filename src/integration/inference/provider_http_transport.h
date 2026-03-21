#pragma once

#include "integration/inference/inference_transport_contracts.h"

namespace life_orchestrator::integration::inference {

class ProviderHttpTransport {
public:
    InferenceTransportResult Run(const InferenceTransportRequest& request) const;
    ProviderHealthCheckResult HealthCheck(const ProviderHealthCheckRequest& request) const;
    std::string name() const { return "provider_http_transport"; }
};

}  // namespace life_orchestrator::integration::inference
