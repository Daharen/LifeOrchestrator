#include "integration/inference/provider_transport_registry.h"

#include "integration/inference/openai_responses_request_builder.h"

namespace life_orchestrator::integration::inference {

ProviderTransportRegistry::ProviderTransportRegistry() : ProviderTransportRegistry(make_default_http_executor()) {}

ProviderTransportRegistry::ProviderTransportRegistry(std::shared_ptr<IHttpExecutor> executor)
    : openai_transport_(executor, true), unsupported_transport_(std::move(executor), false) {}

const ProviderHttpTransport& ProviderTransportRegistry::Resolve(const std::string& provider_name) const {
    return is_openai_like_provider_name(provider_name) ? openai_transport_ : unsupported_transport_;
}

}  // namespace life_orchestrator::integration::inference
