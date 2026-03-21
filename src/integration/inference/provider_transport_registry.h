#pragma once

#include "integration/inference/http_executor_contracts.h"
#include "integration/inference/provider_http_transport.h"

#include <memory>

namespace life_orchestrator::integration::inference {

class ProviderTransportRegistry {
public:
    ProviderTransportRegistry();
    explicit ProviderTransportRegistry(std::shared_ptr<IHttpExecutor> executor);

    const ProviderHttpTransport& Resolve(const std::string& provider_name) const;
private:
    ProviderHttpTransport openai_transport_;
    ProviderHttpTransport unsupported_transport_;
};

}  // namespace life_orchestrator::integration::inference
