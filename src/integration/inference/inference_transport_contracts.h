#pragma once

#include <optional>
#include <string>
#include <vector>

namespace life_orchestrator::integration::inference {

struct InferenceTransportMessage {
    std::string role;
    std::string content;
};

struct InferenceTransportOptions {
    std::string provider_name;
    std::string model_name;
    std::string endpoint_url;
    std::string api_key;
    std::string api_key_reference;
    double temperature = 0.0;
    int max_output_tokens = 0;
};

struct InferenceTransportRequest {
    std::string request_id;
    std::vector<InferenceTransportMessage> messages;
    InferenceTransportOptions options;
};

struct InferenceTransportUsage {
    int input_tokens = 0;
    int output_tokens = 0;
    int total_tokens = 0;
};

struct InferenceTransportError {
    std::string failure_class;
    std::string message;
    int status_code = 0;
    bool retryable = false;
    bool outbound_request_attempted = false;
    std::string failure_stage;
    std::string request_id;
    std::string response_content_type;
    std::string safe_error_summary;
    std::string safe_body_preview;
};

struct InferenceTransportResult {
    bool ok = false;
    std::string provider_name;
    std::string model_name;
    std::string request_id;
    std::string output_text;
    InferenceTransportUsage usage;
    std::optional<InferenceTransportError> error;
};

struct ProviderHealthCheckRequest {
    std::string request_id;
    InferenceTransportOptions options;
};

struct ProviderHealthCheckResult {
    bool ok = false;
    std::string provider_name;
    std::string model_name;
    std::string request_id;
    std::string transport_name;
    bool metadata_loaded = false;
    bool secret_resolved = false;
    bool outbound_request_attempted = false;
    std::string failure_stage;
    int http_status = 0;
    std::string response_request_id;
    std::string response_content_type;
    std::string safe_response_preview;
    std::optional<InferenceTransportResult> inference_result;
    std::optional<InferenceTransportError> error;
};

std::string redact_secret(const std::string& value);
std::string summarize_transport_error(const InferenceTransportError& error);
std::string sanitize_diagnostic_text(const std::string& value, std::size_t max_length = 512);

}  // namespace life_orchestrator::integration::inference
