#include "integration/inference/provider_transport_registry.h"

namespace life_orchestrator::integration::inference {

const ProviderHttpTransport& ProviderTransportRegistry::Resolve(const std::string&) const {
    return http_transport_;
}

}  // namespace life_orchestrator::integration::inference
