#include "integration/inference/provider_http_transport.h"

#include "integration/inference/openai_responses_request_builder.h"
#include "integration/inference/openai_responses_response_parser.h"

namespace life_orchestrator::integration::inference {

ProviderHttpTransport::ProviderHttpTransport(std::shared_ptr<IHttpExecutor> executor, bool supported_provider)
    : executor_(std::move(executor)), supported_provider_(supported_provider) {}

InferenceTransportResult ProviderHttpTransport::Run(const InferenceTransportRequest& request) const {
    InferenceTransportResult result;
    result.provider_name = request.options.provider_name;
    result.model_name = request.options.model_name;
    result.request_id = request.request_id;

    if (!supported_provider_ || !is_openai_like_provider_name(request.options.provider_name)) {
        result.error = InferenceTransportError{"unsupported_provider", "provider is not supported by the current transport registry", 0, false};
        return result;
    }
    if (request.options.api_key.empty()) {
        result.error = InferenceTransportError{"missing_secret", "provider secret could not be resolved", 0, false};
        return result;
    }

    const auto http_request = build_openai_responses_request(request);
    const auto response = executor_->Execute(http_request);
    if (!response.success) {
        result.error = InferenceTransportError{"http_failure", response.transport_error_text.empty() ? "outbound request failed" : response.transport_error_text, response.http_status, true};
        return result;
    }
    if (response.http_status == 429) {
        result.error = InferenceTransportError{"rate_limited", "provider rate limit reached", response.http_status, true};
        return result;
    }
    if (response.http_status < 200 || response.http_status >= 300) {
        result.error = InferenceTransportError{"http_failure", "provider returned non-success status", response.http_status, response.http_status >= 500};
        return result;
    }

    const auto parsed = parse_openai_structured_output_to_key_value(response.body);
    if (!parsed.has_value()) {
        result.error = InferenceTransportError{"schema_parse_failure", "structured response could not be normalized", response.http_status, false};
        return result;
    }

    result.ok = true;
    result.output_text = *parsed;
    result.usage = parse_openai_usage(response.body);
    return result;
}

ProviderHealthCheckResult ProviderHttpTransport::HealthCheck(const ProviderHealthCheckRequest& request) const {
    ProviderHealthCheckResult result;
    result.provider_name = request.options.provider_name;
    result.model_name = request.options.model_name;
    result.request_id = request.request_id;
    result.transport_name = name();
    result.metadata_loaded = !request.options.provider_name.empty() && !request.options.model_name.empty();
    result.secret_resolved = !request.options.api_key.empty();
    result.outbound_request_attempted = supported_provider_ && result.metadata_loaded && result.secret_resolved;
    auto inference_result = Run({request.request_id, {{"system", "Return structured routing output only."}, {"user", "Health check prompt. Return a safe failure or closest command."}}, request.options});
    result.inference_result = inference_result;
    result.ok = inference_result.ok;
    if (!inference_result.ok) result.error = inference_result.error;
    return result;
}

}  // namespace life_orchestrator::integration::inference
