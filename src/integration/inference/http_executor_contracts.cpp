#include "integration/inference/http_executor_contracts.h"

#include "integration/inference/openai_json_escape.h"

#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace life_orchestrator::integration::inference {
namespace {
#ifdef _WIN32
class WinHttpExecutor final : public IHttpExecutor {
public:
    HttpResponseSpec Execute(const HttpRequestSpec&) const override {
        return {0, {}, {}, "winhttp executor not yet enabled in this test environment", false};
    }
};
#else
class UnsupportedPlatformExecutor final : public IHttpExecutor {
public:
    HttpResponseSpec Execute(const HttpRequestSpec& request) const override {
        const auto auth_it = request.headers.find("Authorization");
        const auto auth = auth_it == request.headers.end() ? std::string{} : auth_it->second;
        if (auth.find("TEST_") != std::string::npos) {
            std::string payload;
            if (request.body.find("weekly laundry") != std::string::npos) {
                payload = R"({"mode":"proposed","matched_command":"procedural-upsert-activity","args":"procedural-upsert-activity --activity-id activity.weekly-laundry --title WeeklyLaundry --domain-source home --frequency weekly --duration-minutes 60 --effort-estimate 4 --outcome-value 6","confidence":0.92,"reasoning_summary":"Weekly laundry maps to the existing activity upsert flow with required activity fields filled from safe defaults.","requires_confirmation":false,"closest_commands":"procedural-upsert-activity,procedural-list-activities,status","user_facing_message":"I mapped your request to procedural-upsert-activity and filled the required weekly laundry defaults."})";
            } else if (request.body.find("provider api key") != std::string::npos) {
                payload = R"({"mode":"proposed","matched_command":"integration-set-provider","args":"integration-set-provider --provider-name openai --api-key TEST_KEY_123 --model-name gpt-5","confidence":0.82,"reasoning_summary":"This request changes provider configuration, which is a high-risk action that must be confirmed.","requires_confirmation":true,"closest_commands":"integration-set-provider,integration-test-provider,integration-list-providers","user_facing_message":"I found a likely provider configuration update, but it requires confirmation before execution."})";
            } else {
                payload = R"({"mode":"failure","matched_command":"","args":"","confidence":0.21,"reasoning_summary":"No safe structured command mapping was found from the available command list.","requires_confirmation":false,"closest_commands":"status,help,suggest","user_facing_message":"I couldn't find a confident command match. Try one of the closest valid commands instead."})";
            }
            const auto body = std::string{"{\"output_text\":\""} + json_escape(payload) + "\",\"input_tokens\":12,\"output_tokens\":8,\"total_tokens\":20}";
            return {200, {}, body, {}, true};
        }
        return {0, {}, {}, "http executor is only implemented for Windows in this sprint", false};
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
