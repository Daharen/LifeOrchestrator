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
            response.network_success,
            response.failure_stage,
            response.response_request_id,
            response.win32_error_code,
            sanitize_diagnostic_text(response.win32_error_message),
            response.response_content_type,
            sanitize_diagnostic_text(response.safe_error_summary),
            sanitize_diagnostic_text(response.safe_body_preview)};
}

std::string failure_class_for_stage(const std::string& stage) {
    if (stage == "open_session") return "open_session_failure";
    if (stage == "connect") return "connect_failure";
    if (stage == "open_request") return "open_request_failure";
    if (stage == "send_request") return "send_request_failure";
    if (stage == "receive_response") return "receive_response_failure";
    if (stage == "read_body") return "read_body_failure";
    return "http_failure";
}
}  // namespace

ProviderHttpTransport::ProviderHttpTransport(std::shared_ptr<IHttpExecutor> executor, bool supported_provider)
    : executor_(std::move(executor)), supported_provider_(supported_provider) {}

InferenceTransportResult ProviderHttpTransport::Run(const InferenceTransportRequest& request) const {
    InferenceTransportResult result;
    result.provider_name = request.options.provider_name;
    result.model_name = request.options.model_name;
    result.request_id = request.request_id;

    if (!supported_provider_ || !is_openai_like_provider_name(request.options.provider_name)) {
        result.error = InferenceTransportError{"unsupported_provider", "provider is not supported by the current transport registry", std::nullopt, false, false, {}, {}, std::nullopt, {}, {}, {}, {}};
        return result;
    }
    if (request.options.api_key.empty()) {
        result.error = InferenceTransportError{"missing_secret", "provider secret could not be resolved", std::nullopt, false, false, {}, {}, std::nullopt, {}, {}, {}, {}};
        return result;
    }

    const auto http_request = build_openai_responses_request(request);
    const auto response = executor_->Execute(http_request);
    if (!response.success) {
        const auto stage = response.failure_stage.empty() ? std::string{"send_request"} : response.failure_stage;
        auto failure_class = failure_class_for_stage(stage);
        auto message = response.transport_error_text.empty() ? std::string{"outbound request failed"} : response.transport_error_text;
        auto retryable = true;
        if (failure_class == "open_session_failure" || failure_class == "open_request_failure") retryable = false;
        auto error = make_transport_error(request, response, failure_class, message, retryable);
        error.failure_stage = stage;
        result.error = error;
        return result;
    }

    const auto parsed = parse_openai_response_body(response.body, response.http_status.value_or(0), response.response_request_id, response.safe_body_preview);
    if (!parsed.success) {
        result.error = parsed.error.has_value()
            ? *parsed.error
            : make_transport_error(request, response, response.http_status.has_value() && *response.http_status >= 500 ? "server_error" : "http_failure", "provider response could not be interpreted", response.http_status.has_value() && *response.http_status >= 500);
        if (result.error->response_content_type.empty()) result.error->response_content_type = response.response_content_type;
        if (result.error->request_id.empty()) result.error->request_id = response.response_request_id;
        if (!result.error->win32_error_code.has_value()) result.error->win32_error_code = response.win32_error_code;
        if (result.error->win32_error_message.empty()) result.error->win32_error_message = sanitize_diagnostic_text(response.win32_error_message);
        if (result.error->safe_body_preview.empty()) result.error->safe_body_preview = sanitize_diagnostic_text(response.safe_body_preview);
        if (result.error->failure_stage.empty() && response.http_status.has_value()) result.error->failure_stage = "read_body";
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
    result.outbound_request_attempted = false;
    auto inference_result = Run({request.request_id, {{"system", "Return structured routing output only."}, {"user", "Health check prompt. Return a safe failure or closest command."}}, request.options});
    result.inference_result = inference_result;
    result.ok = inference_result.ok;
    if (!inference_result.ok && inference_result.error.has_value()) {
        result.error = inference_result.error;
        result.failure_stage = inference_result.error->failure_stage;
        result.http_status = inference_result.error->status_code;
        result.response_request_id = inference_result.error->request_id;
        result.win32_error_code = inference_result.error->win32_error_code;
        result.win32_error_message = inference_result.error->win32_error_message;
        result.response_content_type = inference_result.error->response_content_type;
        result.safe_response_preview = inference_result.error->safe_body_preview;
        result.outbound_request_attempted = inference_result.error->outbound_request_attempted;
    } else {
        result.outbound_request_attempted = result.secret_resolved && result.metadata_loaded;
    }
    return result;
}

}  // namespace life_orchestrator::integration::inference
