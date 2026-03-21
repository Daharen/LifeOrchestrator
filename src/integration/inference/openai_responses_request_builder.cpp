#include "integration/inference/openai_responses_request_builder.h"

#include "integration/inference/openai_json_escape.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace life_orchestrator::integration::inference {
namespace {
std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

struct CommandCatalogEntry {
    std::string command_id;
    std::string purpose;
    std::string argument_pattern;
    std::string use_when;
    std::string avoid_when;
};

std::vector<CommandCatalogEntry> command_catalog() {
    return {
        {"help", "Show the supported command surface and exact usage guidance.", "help", "Use for broad capability or command-discovery requests.", "Avoid when the user clearly wants a concrete state lookup or mutation."},
        {"suggest", "Suggest likely exact commands to try next.", "suggest <partial-command>", "Use when the user is exploratory but still asking for command discovery.", "Avoid when a concrete supported command already fits cleanly."},
        {"status", "Show the current system and memory status summary.", "status", "Use for high-level current-state questions about the system.", "Avoid when the user is asking to create, update, or list a specific domain artifact."},
        {"procedural-upsert-activity", "Create or update an activity inventory item such as a recurring household task.", "procedural-upsert-activity --activity-id <id> --title <Title> --domain-source <domain> --frequency <cadence> --duration-minutes <minutes> --effort-estimate <1-10> --outcome-value <1-10>", "Use for create/update task requests when you can ground the required fields safely.", "Avoid when the request is too vague to fill required fields or when the user only wants to list tasks."},
        {"procedural-list-activities", "List activity inventory items.", "procedural-list-activities", "Use when the user wants to review current tasks or activities.", "Avoid when the user wants to create or update an activity."},
        {"behavioral-list-backlog", "List the current behavioral backlog or priorities.", "behavioral-list-backlog", "Use for requests about priorities, backlog, or what needs attention now.", "Avoid when the user wants capabilities help instead of their actual backlog."},
        {"behavioral-list-interventions", "List current behavioral interventions.", "behavioral-list-interventions", "Use for requests about interventions, supports, or active behavior-change guidance.", "Avoid when the user is asking for backlog or activity inventory instead."},
        {"artifact.query", "Query user-facing artifacts such as activity inventory or behavioral backlog summaries.", "artifact.query --artifact-type <activity_inventory|behavioral_backlog|behavioral_interventions|provider_config_summary>", "Use when the user explicitly wants artifact-style summaries or records.", "Avoid when a simpler list/help command is a better fit."}
    };
}

std::string schema_json() {
    return R"({"type":"object","additionalProperties":false,"required":["mode","matched_command","args","confidence","reasoning_summary","requires_confirmation","closest_commands","user_facing_message"],"properties":{"mode":{"type":"string"},"matched_command":{"type":"string"},"args":{"type":"string"},"confidence":{"type":"number"},"reasoning_summary":{"type":"string"},"requires_confirmation":{"type":"boolean"},"closest_commands":{"type":"string"},"user_facing_message":{"type":"string"}}})";
}

std::string grounding_instruction() {
    std::ostringstream out;
    out << "You are the intent router. Return only the canonical routing JSON object with fields mode, matched_command, args, confidence, reasoning_summary, requires_confirmation, closest_commands, user_facing_message. ";
    out << "Choose only from the command catalog below. Do not invent commands. If nothing fits, return mode failure with matched_command empty, args empty, and a helpful grounded user_facing_message. ";
    out << "mode must be exactly proposed or failure. Do not use synonyms. Do not use explanatory prose instead of the canonical value. ";
    out << "Never emit values like invalid, clarification, question, informational, list, assist, or any other mode not in the contract. ";
    out << "matched_command must be empty when mode=failure. ";
    out << "If the request is vague, exploratory, or about broad capabilities, prefer a grounded help-oriented failure or the help command rather than hallucinating an action route. ";
    out << "For mode=proposed, matched_command must be one of the catalog command ids, args must be one canonical CLI-style command string, and closest_commands must be a comma-delimited string of catalog commands.\n";
    out << "Authoritative command catalog:\n";
    for (const auto& entry : command_catalog()) {
        out << "- command_id=" << entry.command_id
            << ";purpose=" << entry.purpose
            << ";args=" << entry.argument_pattern
            << ";use_when=" << entry.use_when
            << ";avoid_when=" << entry.avoid_when << "\n";
    }
    out << "Few-shot examples (canonical outputs only):\n";
    out << "User: Create laundry task\nAssistant: {\"mode\":\"proposed\",\"matched_command\":\"procedural-upsert-activity\",\"args\":\"procedural-upsert-activity --activity-id activity.laundry --title Laundry --domain-source home --frequency weekly --duration-minutes 60 --effort-estimate 4 --outcome-value 6\",\"confidence\":0.90,\"reasoning_summary\":\"Laundry task creation maps to the existing activity upsert flow with safe defaults.\",\"requires_confirmation\":false,\"closest_commands\":\"procedural-upsert-activity,procedural-list-activities\",\"user_facing_message\":\"I mapped that to a grounded laundry activity proposal.\"}\n";
    out << "User: Create a weekly laundry task\nAssistant: {\"mode\":\"proposed\",\"matched_command\":\"procedural-upsert-activity\",\"args\":\"procedural-upsert-activity --activity-id activity.weekly-laundry --title WeeklyLaundry --domain-source home --frequency weekly --duration-minutes 60 --effort-estimate 4 --outcome-value 6\",\"confidence\":0.92,\"reasoning_summary\":\"Weekly laundry can be grounded with deterministic weekly defaults.\",\"requires_confirmation\":false,\"closest_commands\":\"procedural-upsert-activity,procedural-list-activities\",\"user_facing_message\":\"I mapped your request to a weekly laundry activity proposal.\"}\n";
    out << "User: Create a new task for washing sheets and blankets subcategorized under laundry, weekly\nAssistant: {\"mode\":\"proposed\",\"matched_command\":\"procedural-upsert-activity\",\"args\":\"procedural-upsert-activity --activity-id activity.sheets-and-blankets --title SheetsAndBlanketsLaundry --domain-source home.laundry --frequency weekly --duration-minutes 90 --effort-estimate 5 --outcome-value 7\",\"confidence\":0.89,\"reasoning_summary\":\"The request specifies a recurring household task with enough detail to ground the required activity fields.\",\"requires_confirmation\":false,\"closest_commands\":\"procedural-upsert-activity,procedural-list-activities\",\"user_facing_message\":\"I mapped that to a grounded weekly sheets-and-blankets laundry activity proposal.\"}\n";
    out << "User: What can you do?\nAssistant: {\"mode\":\"failure\",\"matched_command\":\"\",\"args\":\"\",\"confidence\":0.42,\"reasoning_summary\":\"This is a broad capability question, so a help-oriented response is safer than inventing an action route.\",\"requires_confirmation\":false,\"closest_commands\":\"help,suggest,status\",\"user_facing_message\":\"Try Help to see supported commands, or ask for a specific status, list, or activity action.\"}\n";
    out << "User: Show my current priorities\nAssistant: {\"mode\":\"proposed\",\"matched_command\":\"behavioral-list-backlog\",\"args\":\"behavioral-list-backlog\",\"confidence\":0.87,\"reasoning_summary\":\"Current priorities map most directly to the behavioral backlog listing.\",\"requires_confirmation\":false,\"closest_commands\":\"behavioral-list-backlog,behavioral-list-interventions,status\",\"user_facing_message\":\"I mapped that to your current backlog priorities.\"}\n";
    return out.str();
}
}  // namespace

std::string default_openai_responses_endpoint() {
    return "https://api.openai.com/v1/responses";
}

bool is_openai_like_provider_name(const std::string& provider_name) {
    const auto lowered = lower_copy(provider_name);
    return lowered == "openai" || lowered == "open-ai" || lowered == "openai-responses";
}

HttpRequestSpec build_openai_responses_request(const InferenceTransportRequest& request) {
    HttpRequestSpec http_request;
    http_request.url = request.options.endpoint_url.empty() ? default_openai_responses_endpoint() : request.options.endpoint_url;
    http_request.method = "POST";
    http_request.timeout_seconds = 30;
    http_request.headers = {{"Authorization", "Bearer " + request.options.api_key},
                            {"Content-Type", "application/json"},
                            {"Accept", "application/json"},
                            {"User-Agent", "LifeOrchestrator/2.0"}};

    std::ostringstream input;
    input << '[';
    for (std::size_t i = 0; i < request.messages.size(); ++i) {
        if (i > 0) input << ',';
        const auto content = request.messages[i].role == "system" ? grounding_instruction() + "\n" + request.messages[i].content : request.messages[i].content;
        input << "{\"role\":\"" << json_escape(request.messages[i].role)
              << "\",\"content\":[{\"type\":\"input_text\",\"text\":\"" << json_escape(content)
              << "\"}]}";
    }
    input << ']';

    std::ostringstream body;
    body << '{';
    body << "\"model\":\"" << json_escape(request.options.model_name) << "\",";
    body << "\"input\":" << input.str() << ',';
    body << "\"text\":{\"format\":{\"type\":\"json_schema\",\"name\":\"intent_routing_result\",\"schema\":" << schema_json() << ",\"strict\":true}}";
    body << '}';
    http_request.body = body.str();
    return http_request;
}

}  // namespace life_orchestrator::integration::inference
