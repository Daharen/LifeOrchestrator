#pragma once

#include "integration/inference/provider_http_transport.h"

namespace life_orchestrator::integration::inference {

class ProviderTransportRegistry {
public:
    const ProviderHttpTransport& Resolve(const std::string& provider_name) const;
private:
    ProviderHttpTransport http_transport_;
};

}  // namespace life_orchestrator::integration::inference
