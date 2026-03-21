#include "integration/inference/inference_transport_client.h"

#include "integration/inference/openai_responses_request_builder.h"

namespace life_orchestrator::integration::inference {

InferenceTransportClient::InferenceTransportClient(ProviderTransportRegistry registry) : registry_(std::move(registry)) {}
InferenceTransportClient::InferenceTransportClient(std::shared_ptr<IHttpExecutor> executor) : registry_(ProviderTransportRegistry(std::move(executor))) {}

InferenceTransportOptions InferenceTransportClient::BuildOptions(const core::IntegrationConfigurationRecord& record,
                                                                 const std::string& api_key) const {
    InferenceTransportOptions options;
    options.provider_name = record.integration_id;
    options.model_name = record.non_secret_settings.contains("model_name") ? record.non_secret_settings.at("model_name") : std::string{};
    options.endpoint_url = record.non_secret_settings.contains("endpoint_url") ? record.non_secret_settings.at("endpoint_url") : std::string{};
    if (options.endpoint_url.empty() && is_openai_like_provider_name(options.provider_name)) options.endpoint_url = default_openai_responses_endpoint();
    options.api_key = api_key;
    options.api_key_reference = record.credential_reference;
    return options;
}

InferenceTransportResult InferenceTransportClient::Interpret(const core::IntegrationConfigurationRecord& record,
                                                             const std::string& api_key,
                                                             const std::string& request_id,
                                                             const std::vector<InferenceTransportMessage>& messages) const {
    return registry_.Resolve(record.integration_id).Run({request_id, messages, BuildOptions(record, api_key)});
}

ProviderHealthCheckResult InferenceTransportClient::CheckProvider(const core::IntegrationConfigurationRecord& record,
                                                                  const std::string& api_key,
                                                                  const std::string& request_id) const {
    return registry_.Resolve(record.integration_id).HealthCheck({request_id, BuildOptions(record, api_key)});
}

}  // namespace life_orchestrator::integration::inference
