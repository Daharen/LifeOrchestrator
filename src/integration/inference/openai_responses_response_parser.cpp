#include "integration/inference/openai_responses_response_parser.h"

#include "integration/inference/openai_json_escape.h"

#include <sstream>

namespace life_orchestrator::integration::inference {
namespace {
std::string trim_quotes(std::string value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') return value.substr(1, value.size() - 2);
    return value;
}

std::string scalar_field(const std::string& json, const std::string& key, const std::string& fallback = {}) {
    if (const auto string_value = json_extract_string_field(json, key); string_value.has_value()) return *string_value;
    if (const auto raw_value = json_extract_raw_field(json, key); raw_value.has_value()) return trim_quotes(*raw_value);
    return fallback;
}

std::optional<InferenceTransportError> parse_openai_error(const std::string& response_body,
                                                          int http_status,
                                                          const std::string& request_id,
                                                          const std::string& body_preview) {
    const auto raw_error = json_extract_raw_field(response_body, "error");
    if (!raw_error.has_value()) return std::nullopt;

    const auto message = scalar_field(*raw_error, "message", "provider returned an error body");
    const auto type = scalar_field(*raw_error, "type");
    const auto code = scalar_field(*raw_error, "code");

    std::string failure_class = "http_failure";
    bool retryable = http_status >= 500;
    if (http_status == 400) failure_class = "bad_request";
    else if (http_status == 401 || http_status == 403) failure_class = "authentication_failure";
    else if (http_status == 429) {
        failure_class = "rate_limited";
        retryable = true;
    } else if (http_status >= 500) {
        failure_class = "server_error";
        retryable = true;
    }

    std::string summary = message;
    if (!type.empty()) summary += " type=" + type;
    if (!code.empty()) summary += " code=" + code;

    return InferenceTransportError{failure_class,
                                   message,
                                   std::optional<int>{http_status},
                                   retryable,
                                   http_status > 0,
                                   http_status > 0 ? "receive_response" : std::string{},
                                   request_id,
                                   std::nullopt,
                                   {},
                                   "application/json",
                                   sanitize_diagnostic_text(summary),
                                   sanitize_diagnostic_text(body_preview.empty() ? response_body : body_preview)};
}
}  // namespace

InferenceTransportUsage parse_openai_usage(const std::string& response_body) {
    InferenceTransportUsage usage;
    if (const auto raw = json_extract_raw_field(response_body, "input_tokens"); raw.has_value()) usage.input_tokens = std::stoi(*raw);
    if (const auto raw = json_extract_raw_field(response_body, "output_tokens"); raw.has_value()) usage.output_tokens = std::stoi(*raw);
    if (const auto raw = json_extract_raw_field(response_body, "total_tokens"); raw.has_value()) usage.total_tokens = std::stoi(*raw);
    return usage;
}

std::optional<std::string> parse_openai_structured_output_to_key_value(const std::string& response_body) {
    std::string structured_json;
    if (const auto direct = json_extract_raw_field(response_body, "mode"); direct.has_value()) {
        structured_json = response_body;
    } else if (const auto text_value = json_extract_string_field(response_body, "text"); text_value.has_value()) {
        structured_json = *text_value;
    } else if (const auto output_text = json_extract_string_field(response_body, "output_text"); output_text.has_value()) {
        structured_json = *output_text;
    } else {
        return std::nullopt;
    }

    const auto mode = scalar_field(structured_json, "mode");
    const auto matched_command = scalar_field(structured_json, "matched_command");
    const auto args = scalar_field(structured_json, "args");
    const auto confidence = scalar_field(structured_json, "confidence", "0");
    const auto reasoning_summary = scalar_field(structured_json, "reasoning_summary");
    const auto requires_confirmation = scalar_field(structured_json, "requires_confirmation", "false");
    const auto closest_commands = scalar_field(structured_json, "closest_commands");
    const auto user_facing_message = scalar_field(structured_json, "user_facing_message");
    if (mode.empty() && matched_command.empty() && user_facing_message.empty()) return std::nullopt;

    std::ostringstream out;
    out << "mode=" << mode << '\n'
        << "matched_command=" << matched_command << '\n'
        << "args=" << args << '\n'
        << "confidence=" << confidence << '\n'
        << "reasoning_summary=" << reasoning_summary << '\n'
        << "requires_confirmation=" << requires_confirmation << '\n'
        << "closest_commands=" << closest_commands << '\n'
        << "user_facing_message=" << user_facing_message << '\n';
    return out.str();
}

OpenAIResponseParseResult parse_openai_response_body(const std::string& response_body, int http_status, const std::string& request_id, const std::string& body_preview) {
    OpenAIResponseParseResult result;
    result.usage = parse_openai_usage(response_body);

    if (http_status < 200 || http_status >= 300) {
        if (const auto error = parse_openai_error(response_body, http_status, request_id, body_preview); error.has_value()) {
            result.error = error;
        } else {
            std::string failure_class = "http_failure";
            bool retryable = http_status >= 500;
            if (http_status == 400) failure_class = "bad_request";
            else if (http_status == 401 || http_status == 403) failure_class = "authentication_failure";
            else if (http_status == 429) {
                failure_class = "rate_limited";
                retryable = true;
            } else if (http_status >= 500) failure_class = "server_error";
            result.error = InferenceTransportError{failure_class, "provider returned non-success status", std::optional<int>{http_status}, retryable, http_status > 0, http_status > 0 ? "receive_response" : std::string{}, request_id, std::nullopt, {}, {}, {}, sanitize_diagnostic_text(body_preview.empty() ? response_body : body_preview)};
        }
        return result;
    }

    const auto normalized = parse_openai_structured_output_to_key_value(response_body);
    if (!normalized.has_value()) {
        const bool json_like = response_body.find('{') != std::string::npos || response_body.find('[') != std::string::npos;
        result.error = InferenceTransportError{"schema_parse_failure",
                                               json_like ? "successful HTTP response did not match expected OpenAI structured response schema"
                                                         : "successful HTTP response body was not parseable JSON",
                                               std::optional<int>{http_status},
                                               false,
                                               true,
                                               "read_body",
                                               request_id,
                                               std::nullopt,
                                               {},
                                               "application/json",
                                               json_like ? "2xx response missing expected structured content" : "2xx response was non-JSON or malformed",
                                               sanitize_diagnostic_text(body_preview.empty() ? response_body : body_preview)};
        return result;
    }

    result.success = true;
    result.normalized_output = *normalized;
    return result;
}

}  // namespace life_orchestrator::integration::inference
