#include "integration/inference/openai_responses_response_parser.h"

#include "integration/inference/openai_json_escape.h"

#include <algorithm>
#include <sstream>
#include <vector>

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

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trim_copy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

std::string normalize_mode(const std::string& value) {
    const auto lowered = lower_copy(value);
    if (lowered == "command" || lowered == "proposal" || lowered == "success") return "proposed";
    if (lowered == "no_match" || lowered == "error") return "failure";
    return value;
}

std::string normalize_bool_like(const std::string& value) {
    const auto lowered = lower_copy(value);
    if (lowered == "true" || lowered == "1" || lowered == "yes") return "true";
    if (lowered == "false" || lowered == "0" || lowered == "no" || lowered.empty() || lowered == "null") return "false";
    return value;
}

std::string normalize_confidence(const std::string& value) {
    try {
        if (value.empty() || lower_copy(value) == "null") return "0";
        auto parsed = std::stod(value);
        if (parsed < 0.0) parsed = 0.0;
        if (parsed > 1.0) parsed = 1.0;
        std::ostringstream out;
        out << parsed;
        return out.str();
    } catch (...) {
        return value;
    }
}

std::string normalize_command_alias(const std::string& value) {
    const auto trimmed = trim_copy(value);
    if (trimmed == "create_activity") return "procedural-upsert-activity";
    if (trimmed == "create-activity") return "procedural-upsert-activity";
    if (trimmed == "list_activities") return "procedural-list-activities";
    if (trimmed == "list_backlog") return "behavioral-list-backlog";
    if (trimmed == "list_interventions") return "behavioral-list-interventions";
    if (trimmed == "show_priorities") return "behavioral-list-backlog";
    return trimmed;
}

std::vector<std::string> parse_string_array(const std::string& raw) {
    std::vector<std::string> values;
    if (raw.size() < 2 || raw.front() != '[' || raw.back() != ']') return values;
    std::size_t cursor = 0;
    while ((cursor = raw.find('"', cursor)) != std::string::npos) {
        const auto end = raw.find('"', cursor + 1);
        if (end == std::string::npos) break;
        values.push_back(raw.substr(cursor + 1, end - cursor - 1));
        cursor = end + 1;
    }
    return values;
}

std::string normalize_command_like_field(const std::string& structured_json, const std::string& key) {
    if (const auto string_value = json_extract_string_field(structured_json, key); string_value.has_value()) return normalize_command_alias(*string_value);
    if (const auto raw_value = json_extract_raw_field(structured_json, key); raw_value.has_value()) return normalize_command_alias(trim_quotes(*raw_value));
    return {};
}

std::string normalize_args(const std::string& structured_json, const std::string& matched_command) {
    if (const auto string_value = json_extract_string_field(structured_json, "args"); string_value.has_value()) return trim_copy(*string_value);
    if (const auto raw_value = json_extract_raw_field(structured_json, "args"); raw_value.has_value()) {
        const auto raw = trim_copy(*raw_value);
        if (raw == "null") return {};
        if (!raw.empty() && raw.front() == '[' && raw.back() == ']') {
            auto parts = parse_string_array(raw);
            if (!parts.empty()) {
                if (!matched_command.empty()) parts.front() = normalize_command_alias(parts.front());
                std::ostringstream out;
                for (std::size_t i = 0; i < parts.size(); ++i) {
                    if (i > 0) out << ' ';
                    out << parts[i];
                }
                return out.str();
            }
            return {};
        }
        return trim_quotes(raw);
    }
    return {};
}

std::string normalize_closest_commands(const std::string& structured_json) {
    if (const auto string_value = json_extract_string_field(structured_json, "closest_commands"); string_value.has_value()) return *string_value;
    if (const auto raw_value = json_extract_raw_field(structured_json, "closest_commands"); raw_value.has_value()) {
        auto raw = *raw_value;
        if (raw == "null" || raw == "[]") return {};
        if (!raw.empty() && raw.front() == '[' && raw.back() == ']') {
            const auto parts = parse_string_array(raw);
            std::ostringstream out;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) out << ',';
                out << normalize_command_alias(parts[i]);
            }
            return out.str();
        }
        return trim_quotes(raw);
    }
    return {};
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

    const auto mode = normalize_mode(scalar_field(structured_json, "mode"));
    const auto matched_command = normalize_command_like_field(structured_json, "matched_command");
    const auto args = normalize_args(structured_json, matched_command);
    const auto confidence = normalize_confidence(scalar_field(structured_json, "confidence", "0"));
    const auto reasoning_summary = scalar_field(structured_json, "reasoning_summary");
    const auto requires_confirmation = normalize_bool_like(scalar_field(structured_json, "requires_confirmation", "false"));
    const auto closest_commands = normalize_closest_commands(structured_json);
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
