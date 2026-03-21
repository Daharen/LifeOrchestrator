#include "integration/inference/provider_http_transport.h"

#include "integration/inference/openai_responses_request_builder.h"
#include "integration/inference/openai_responses_response_parser.h"

namespace life_orchestrator::integration::inference {
namespace {
InferenceTransportError make_transport_error(const InferenceTransportRequest& request,
                                             const HttpResponseSpec& response,
                                             std::string failure_class,
                                             std::string message,
                                             bool retryable) {
    return {std::move(failure_class),
            std::move(message),
            response.http_status,
            retryable,
            response.network_success || response.http_status > 0,
            response.failure_stage,
            response.response_request_id,
            response.response_content_type,
            sanitize_diagnostic_text(response.safe_error_summary),
            sanitize_diagnostic_text(response.safe_body_preview)};
}
}

ProviderHttpTransport::ProviderHttpTransport(std::shared_ptr<IHttpExecutor> executor, bool supported_provider)
    : executor_(std::move(executor)), supported_provider_(supported_provider) {}

InferenceTransportResult ProviderHttpTransport::Run(const InferenceTransportRequest& request) const {
    InferenceTransportResult result;
    result.provider_name = request.options.provider_name;
    result.model_name = request.options.model_name;
    result.request_id = request.request_id;

    if (!supported_provider_ || !is_openai_like_provider_name(request.options.provider_name)) {
        result.error = InferenceTransportError{"unsupported_provider", "provider is not supported by the current transport registry", 0, false, false, {}, {}, {}, {}, {}};
        return result;
    }
    if (request.options.api_key.empty()) {
        result.error = InferenceTransportError{"missing_secret", "provider secret could not be resolved", 0, false, false, {}, {}, {}, {}, {}};
        return result;
    }

    const auto http_request = build_openai_responses_request(request);
    const auto response = executor_->Execute(http_request);
    if (!response.success) {
        const auto stage = response.failure_stage.empty() ? std::string{"send_request"} : response.failure_stage;
        auto failure_class = std::string{"http_failure"};
        auto message = response.transport_error_text.empty() ? std::string{"outbound request failed"} : response.transport_error_text;
        auto retryable = true;
        if (stage == "connect") {
            failure_class = "dns_connect_failure";
            message = "DNS or connect failure during outbound request";
        } else if (stage == "open_session" || stage == "open_request" || stage == "send_request") {
            failure_class = "tls_or_send_failure";
            message = response.transport_error_text.empty() ? "secure channel or request send failure" : response.transport_error_text;
        } else if (stage == "receive_response" || stage == "read_body") {
            failure_class = "timeout";
            message = response.transport_error_text.empty() ? "provider response timed out or stream failed" : response.transport_error_text;
        }
        auto error = make_transport_error(request, response, failure_class, message, retryable);
        error.failure_stage = stage;
        result.error = error;
        return result;
    }

    const auto parsed = parse_openai_response_body(response.body, response.http_status, response.response_request_id, response.safe_body_preview);
    if (!parsed.success) {
        result.error = parsed.error.has_value()
            ? *parsed.error
            : make_transport_error(request, response, response.http_status >= 500 ? "server_error" : "http_failure", "provider response could not be interpreted", response.http_status >= 500);
        if (result.error->response_content_type.empty()) result.error->response_content_type = response.response_content_type;
        if (result.error->request_id.empty()) result.error->request_id = response.response_request_id;
        if (result.error->safe_body_preview.empty()) result.error->safe_body_preview = sanitize_diagnostic_text(response.safe_body_preview);
        if (result.error->failure_stage.empty() && response.http_status > 0) result.error->failure_stage = "read_body";
        return result;
    }

    result.ok = true;
    result.output_text = parsed.normalized_output;
    result.usage = parsed.usage;
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
    if (!inference_result.ok && inference_result.error.has_value()) {
        result.error = inference_result.error;
        result.failure_stage = inference_result.error->failure_stage;
        result.http_status = inference_result.error->status_code;
        result.response_request_id = inference_result.error->request_id;
        result.response_content_type = inference_result.error->response_content_type;
        result.safe_response_preview = inference_result.error->safe_body_preview;
    }
    return result;
}

}  // namespace life_orchestrator::integration::inference
