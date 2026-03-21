#include "integration/inference/inference_transport_contracts.h"

#include <algorithm>
#include <cctype>

namespace life_orchestrator::integration::inference {
namespace {
constexpr std::size_t k_default_preview_limit = 512;

bool is_sensitive_header_name(const std::string& lower_name) {
    return lower_name.find("authorization") != std::string::npos || lower_name.find("api-key") != std::string::npos ||
           lower_name.find("token") != std::string::npos || lower_name.find("secret") != std::string::npos ||
           lower_name.find("bearer") != std::string::npos;
}
}  // namespace

std::string redact_secret(const std::string& value) {
    if (value.empty()) return "unset";
    if (value.size() <= 4) return "****";
    return value.substr(0, 2) + "***" + value.substr(value.size() - 2);
}

std::string sanitize_diagnostic_text(const std::string& value, std::size_t max_length) {
    std::string sanitized;
    sanitized.reserve(std::min(value.size(), max_length));

    std::string lower_window;
    lower_window.reserve(64);
    bool skipping_line = false;
    for (char ch : value) {
        if (sanitized.size() >= max_length) break;
        if (ch == '\r') continue;
        if (ch == '\n') {
            skipping_line = false;
            lower_window.clear();
            sanitized.push_back('\n');
            continue;
        }
        const unsigned char uch = static_cast<unsigned char>(ch);
        const char lowered = static_cast<char>(std::tolower(uch));
        lower_window.push_back(lowered);
        if (lower_window.size() > 64) lower_window.erase(0, lower_window.size() - 64);
        if (!skipping_line && (is_sensitive_header_name(lower_window) || lower_window.find("sk-") != std::string::npos || lower_window.find("test_key") != std::string::npos || lower_window.find("bearer ") != std::string::npos)) {
            skipping_line = true;
            if (sanitized.size() + 10 <= max_length) sanitized += "[redacted]";
            continue;
        }
        if (skipping_line) continue;
        sanitized.push_back(std::isprint(uch) || ch == '\t' ? ch : '?');
    }

    if (value.size() > sanitized.size()) {
        if (sanitized.size() + 3 > max_length && sanitized.size() >= 3) sanitized.resize(max_length - 3);
        if (sanitized.size() < max_length) sanitized += "...";
    }
    return sanitized;
}

std::string summarize_transport_error(const InferenceTransportError& error) {
    std::string summary = error.failure_class + ":" + sanitize_diagnostic_text(error.message, k_default_preview_limit / 2);
    if (error.status_code.has_value()) summary += " status=" + std::to_string(*error.status_code);
    if (!error.failure_stage.empty()) summary += " stage=" + error.failure_stage;
    if (error.win32_error_code.has_value()) summary += " win32_error=" + std::to_string(*error.win32_error_code);
    if (!error.win32_error_message.empty()) summary += " win32_message=" + sanitize_diagnostic_text(error.win32_error_message, 160);
    if (!error.request_id.empty()) summary += " request_id=" + sanitize_diagnostic_text(error.request_id, 64);
    if (!error.safe_error_summary.empty()) summary += " detail=" + sanitize_diagnostic_text(error.safe_error_summary, k_default_preview_limit / 2);
    if (!error.safe_body_preview.empty()) summary += " preview=" + sanitize_diagnostic_text(error.safe_body_preview, 160);
    return summary;
}

}  // namespace life_orchestrator::integration::inference
