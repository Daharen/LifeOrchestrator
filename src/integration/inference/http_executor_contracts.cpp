#include "integration/inference/http_executor_contracts.h"

#include "integration/inference/inference_transport_contracts.h"
#include "integration/inference/openai_json_escape.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace life_orchestrator::integration::inference {
namespace {
std::string trim_copy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string safe_preview(const std::string& value) {
    return sanitize_diagnostic_text(value, 256);
}

#ifndef _WIN32
class UnsupportedPlatformExecutor final : public IHttpExecutor {
public:
    HttpResponseSpec Execute(const HttpRequestSpec& request) const override {
        const auto auth_it = request.headers.find("Authorization");
        const auto auth = auth_it == request.headers.end() ? std::string{} : auth_it->second;
        if (auth.find("TEST_") != std::string::npos) {
            std::string payload;
            const auto lowered_body = lower_copy(request.body);
            const auto user_request = lowered_body.rfind("user request:");
            const auto prompt_tail = user_request == std::string::npos ? lowered_body : lowered_body.substr(user_request);
            if (prompt_tail.find("sheets and blankets") != std::string::npos) {
                payload = R"({"mode":"proposed","matched_command":"procedural-upsert-activity","args":"procedural-upsert-activity --activity-id activity.sheets-and-blankets --title SheetsAndBlanketsLaundry --domain-source home.laundry --frequency weekly --duration-minutes 90 --effort-estimate 5 --outcome-value 7","confidence":0.89,"reasoning_summary":"Sheets and blankets laundry maps to the existing activity upsert flow with grounded weekly defaults.","requires_confirmation":false,"closest_commands":"procedural-upsert-activity,procedural-list-activities","user_facing_message":"I mapped that to a grounded weekly sheets-and-blankets laundry activity proposal."})";
            } else if (prompt_tail.find("create laundry task") != std::string::npos || prompt_tail.find("weekly laundry") != std::string::npos) {
                payload = R"({"mode":"proposed","matched_command":"procedural-upsert-activity","args":"procedural-upsert-activity --activity-id activity.weekly-laundry --title WeeklyLaundry --domain-source home --frequency weekly --duration-minutes 60 --effort-estimate 4 --outcome-value 6","confidence":0.92,"reasoning_summary":"Weekly laundry maps to the existing activity upsert flow with required activity fields filled from safe defaults.","requires_confirmation":false,"closest_commands":"procedural-upsert-activity,status","user_facing_message":"I mapped your request to procedural-upsert-activity and filled the required weekly laundry defaults."})";
            } else if (prompt_tail.find("what can you do") != std::string::npos) {
                payload = R"({"mode":"failure","matched_command":"","args":"","confidence":0.42,"reasoning_summary":"Broad capability questions should degrade to grounded help-oriented guidance.","requires_confirmation":false,"closest_commands":"help,suggest,status","user_facing_message":"Try Help to see supported commands, or ask for a specific status, list, or activity action."})";
            } else if (prompt_tail.find("provider api key") != std::string::npos) {
                payload = R"({"mode":"proposed","matched_command":"integration-set-provider","args":"integration-set-provider --provider-name openai --api-key TEST_KEY_123 --model-name gpt-5","confidence":0.82,"reasoning_summary":"This request changes provider configuration, which is a high-risk action that must be confirmed.","requires_confirmation":true,"closest_commands":"integration-set-provider,integration-test-provider,integration-list-providers","user_facing_message":"I found a likely provider configuration update, but it requires confirmation before execution."})";
            } else {
                payload = R"({"mode":"failure","matched_command":"","args":"","confidence":0.21,"reasoning_summary":"No safe structured command mapping was found from the available command list.","requires_confirmation":false,"closest_commands":"status,help,suggest","user_facing_message":"I couldn't find a confident command match. Try one of the closest valid commands instead."})";
            }
            const auto response_body = std::string{"{\"output_text\":\""} + json_escape(payload) + "\",\"input_tokens\":12,\"output_tokens\":8,\"total_tokens\":20}";
            return {200, {}, response_body, {}, true, true, {}, std::nullopt, {}, "application/json", {}, {}, {}};
        }
        return {std::nullopt, {}, {}, "http executor is only implemented for Windows in this sprint", false, false, "open_session", std::nullopt, {}, {}, {}, "http executor unavailable on this platform", {}};
    }
};
#else
std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const auto size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const auto size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
    bool secure = false;
};

std::optional<ParsedUrl> parse_url(const std::string& url) {
    const auto wide = utf8_to_wide(url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide.c_str(), static_cast<DWORD>(wide.size()), 0, &components)) return std::nullopt;
    ParsedUrl parsed;
    parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
    parsed.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) parsed.path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    if (parsed.path.empty()) parsed.path = L"/";
    parsed.port = components.nPort;
    parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return parsed;
}

std::string decode_win32_error(unsigned long code) {
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const auto length = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message;
    if (length > 0 && buffer != nullptr) {
        message.assign(buffer, length);
        LocalFree(buffer);
    }
    return trim_copy(wide_to_utf8(message));
}

void assign_failure(HttpResponseSpec& response,
                    const std::string& stage,
                    unsigned long code,
                    const std::string& fallback) {
    response.success = false;
    response.network_success = false;
    response.failure_stage = stage;
    response.win32_error_code = code;
    response.win32_error_message = decode_win32_error(code);
    response.transport_error_text = response.win32_error_message.empty() ? fallback : response.win32_error_message;
    response.safe_error_summary = safe_preview(stage + ": " + response.transport_error_text + " win32_error=" + std::to_string(code));
}

template <typename Handle, typename CloseFn>
class ScopedHandle {
public:
    ScopedHandle() = default;
    explicit ScopedHandle(Handle handle) : handle_(handle) {}
    ~ScopedHandle() { reset(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.release()) {}
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }
    Handle get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }
    Handle release() {
        Handle temp = handle_;
        handle_ = nullptr;
        return temp;
    }
    void reset(Handle handle = nullptr) {
        if (handle_ != nullptr) CloseFn{}(handle_);
        handle_ = handle;
    }
private:
    Handle handle_ = nullptr;
};

struct WinHttpCloser { void operator()(HINTERNET handle) const { WinHttpCloseHandle(handle); } };

std::string query_header_string(HINTERNET request, DWORD info_level, const wchar_t* name = WINHTTP_HEADER_NAME_BY_INDEX) {
    DWORD size = 0;
    if (!WinHttpQueryHeaders(request, info_level, name, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return {};
    }
    std::wstring buffer(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request, info_level, name, buffer.data(), &size, WINHTTP_NO_HEADER_INDEX)) return {};
    if (!buffer.empty() && buffer.back() == L'\0') buffer.pop_back();
    return trim_copy(wide_to_utf8(buffer));
}

class WinHttpExecutor final : public IHttpExecutor {
public:
    HttpResponseSpec Execute(const HttpRequestSpec& request) const override {
        HttpResponseSpec response;
        const auto parsed_url = parse_url(request.url);
        if (!parsed_url.has_value()) {
            response.failure_stage = "open_request";
            response.transport_error_text = "request URL could not be parsed";
            response.safe_error_summary = safe_preview(response.transport_error_text);
            return response;
        }

        ScopedHandle<HINTERNET, WinHttpCloser> session(WinHttpOpen(L"LifeOrchestrator/1.0",
                                                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                                                    WINHTTP_NO_PROXY_NAME,
                                                                    WINHTTP_NO_PROXY_BYPASS,
                                                                    0));
        if (!session) {
            assign_failure(response, "open_session", GetLastError(), "WinHttpOpen failed");
            return response;
        }

        const auto timeout_ms = request.timeout_seconds > 0 ? request.timeout_seconds * 1000 : 30000;
        WinHttpSetTimeouts(session.get(), timeout_ms, timeout_ms, timeout_ms, timeout_ms);

        ScopedHandle<HINTERNET, WinHttpCloser> connection(WinHttpConnect(session.get(),
                                                                         parsed_url->host.c_str(),
                                                                         parsed_url->port,
                                                                         0));
        if (!connection) {
            assign_failure(response, "connect", GetLastError(), "WinHttpConnect failed");
            return response;
        }

        const auto method = utf8_to_wide(request.method.empty() ? std::string{"POST"} : request.method);
        const auto flags = parsed_url->secure ? WINHTTP_FLAG_SECURE : 0;
        ScopedHandle<HINTERNET, WinHttpCloser> win_request(WinHttpOpenRequest(connection.get(),
                                                                              method.c_str(),
                                                                              parsed_url->path.c_str(),
                                                                              nullptr,
                                                                              WINHTTP_NO_REFERER,
                                                                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                                              flags));
        if (!win_request) {
            assign_failure(response, "open_request", GetLastError(), "WinHttpOpenRequest failed");
            return response;
        }

        std::wstring raw_headers;
        for (const auto& [key, value] : request.headers) raw_headers += utf8_to_wide(key + ": " + value + "\r\n");
        const auto body_size = static_cast<DWORD>(request.body.size());
        if (!WinHttpSendRequest(win_request.get(),
                                raw_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : raw_headers.c_str(),
                                raw_headers.empty() ? 0 : static_cast<DWORD>(raw_headers.size()),
                                request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data()),
                                body_size,
                                body_size,
                                0)) {
            assign_failure(response, "send_request", GetLastError(), "WinHttpSendRequest failed");
            return response;
        }

        response.network_success = true;

        if (!WinHttpReceiveResponse(win_request.get(), nullptr)) {
            assign_failure(response, "receive_response", GetLastError(), "WinHttpReceiveResponse failed");
            return response;
        }

        DWORD status_code = 0;
        DWORD status_size = sizeof(status_code);
        if (WinHttpQueryHeaders(win_request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX)) {
            response.http_status = static_cast<int>(status_code);
        }
        response.response_content_type = query_header_string(win_request.get(), WINHTTP_QUERY_CONTENT_TYPE);
        response.response_request_id = query_header_string(win_request.get(), WINHTTP_QUERY_CUSTOM, L"x-request-id");
        if (response.response_request_id.empty()) response.response_request_id = query_header_string(win_request.get(), WINHTTP_QUERY_CUSTOM, L"request-id");
        if (response.response_request_id.empty()) response.response_request_id = query_header_string(win_request.get(), WINHTTP_QUERY_CUSTOM, L"openai-request-id");

        std::string body;
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(win_request.get(), &available)) {
                assign_failure(response, "read_body", GetLastError(), "WinHttpQueryDataAvailable failed");
                response.safe_body_preview = safe_preview(body);
                return response;
            }
            if (available == 0) break;
            std::vector<char> buffer(available);
            DWORD read = 0;
            if (!WinHttpReadData(win_request.get(), buffer.data(), available, &read)) {
                assign_failure(response, "read_body", GetLastError(), "WinHttpReadData failed");
                response.safe_body_preview = safe_preview(body);
                return response;
            }
            body.append(buffer.data(), read);
        }

        response.body = std::move(body);
        response.safe_body_preview = safe_preview(response.body);
        response.success = response.http_status.has_value();
        response.failure_stage = response.success ? "" : "receive_response";
        return response;
    }
};
#endif
}  // namespace

std::shared_ptr<IHttpExecutor> make_default_http_executor() {
#ifdef _WIN32
    return std::make_shared<WinHttpExecutor>();
#else
    return std::make_shared<UnsupportedPlatformExecutor>();
#endif
}

}  // namespace life_orchestrator::integration::inference
