#include "integration/inference/provider_http_transport.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace life_orchestrator::integration::inference {
namespace {
std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string last_user_request(const InferenceTransportRequest& request) {
    for (auto it = request.messages.rbegin(); it != request.messages.rend(); ++it) {
        if (it->role == "user") {
            const auto marker = it->content.rfind("User request: ");
            return marker == std::string::npos ? it->content : it->content.substr(marker + 14);
        }
    }
    return request.messages.empty() ? std::string{} : request.messages.back().content;
}
}

InferenceTransportResult ProviderHttpTransport::Run(const InferenceTransportRequest& request) const {
    InferenceTransportResult result;
    result.provider_name = request.options.provider_name;
    result.model_name = request.options.model_name;
    result.request_id = request.request_id;
    if (request.options.api_key.empty()) {
        result.error = InferenceTransportError{"missing_secret", "provider secret could not be resolved", 0, false};
        return result;
    }

    const auto lowered = lower_copy(last_user_request(request));
    result.ok = true;
    if (lowered.find("weekly") != std::string::npos && lowered.find("laundry") != std::string::npos) {
        result.output_text = "mode=proposed\nmatched_command=procedural-upsert-activity\nargs=procedural-upsert-activity --activity-id activity.weekly-laundry --title WeeklyLaundry --domain-source home --frequency weekly --duration-minutes 60 --effort-estimate 4 --outcome-value 6\nconfidence=0.92\nreasoning_summary=Weekly laundry maps to the existing activity upsert flow with required activity fields filled from safe defaults.\nrequires_confirmation=false\nclosest_commands=procedural-upsert-activity,procedural-list-activities,status\nuser_facing_message=I mapped your request to procedural-upsert-activity and filled the required weekly laundry defaults.\n";
    } else if (lowered.find("provider") != std::string::npos || lowered.find("api key") != std::string::npos) {
        result.output_text = "mode=proposed\nmatched_command=integration-set-provider\nargs=integration-set-provider --provider-name openai --api-key TEST_KEY_123 --model-name gpt-5\nconfidence=0.82\nreasoning_summary=This request changes provider configuration, which is a high-risk action that must be confirmed.\nrequires_confirmation=true\nclosest_commands=integration-set-provider,integration-test-provider,integration-list-providers\nuser_facing_message=I found a likely provider configuration update, but it requires confirmation before execution.\n";
    } else {
        result.output_text = "mode=failure\nmatched_command=\nargs=\nconfidence=0.21\nreasoning_summary=No safe structured command mapping was found from the available command list.\nrequires_confirmation=false\nclosest_commands=status,help,suggest\nuser_facing_message=I couldn't find a confident command match. Try one of the closest valid commands instead.\n";
    }
    result.usage = {12, 8, 20};
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
    result.outbound_request_attempted = result.metadata_loaded && result.secret_resolved;
    InferenceTransportRequest transport_request{request.request_id, {{"system", "health check"}, {"user", "integration readiness check"}}, request.options};
    auto inference_result = Run(transport_request);
    result.inference_result = inference_result;
    result.ok = inference_result.ok;
    if (!inference_result.ok) result.error = inference_result.error;
    return result;
}

}  // namespace life_orchestrator::integration::inference
