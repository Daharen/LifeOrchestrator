#pragma once

#include <map>
#include <memory>
#include <string>

namespace life_orchestrator::integration::inference {

struct HttpRequestSpec {
    std::string url;
    std::string method;
    std::map<std::string, std::string> headers;
    std::string body;
    int timeout_seconds = 30;
};

struct HttpResponseSpec {
    int http_status = 0;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string transport_error_text;
    bool success = false;
    bool network_success = false;
    std::string failure_stage;
    std::string response_content_type;
    std::string response_request_id;
    std::string safe_error_summary;
    std::string safe_body_preview;
};

class IHttpExecutor {
public:
    virtual ~IHttpExecutor() = default;
    virtual HttpResponseSpec Execute(const HttpRequestSpec& request) const = 0;
};

std::shared_ptr<IHttpExecutor> make_default_http_executor();

}  // namespace life_orchestrator::integration::inference
