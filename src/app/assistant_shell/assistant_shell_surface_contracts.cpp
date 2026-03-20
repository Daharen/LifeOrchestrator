#include "app/assistant_shell/assistant_shell_surface_contracts.h"

namespace life_orchestrator::app::assistant_shell {

std::string to_string(AssistantShellMessageBlockType type) {
    switch (type) {
        case AssistantShellMessageBlockType::UserText: return "user_text";
        case AssistantShellMessageBlockType::AssistantResponse: return "assistant_response";
        case AssistantShellMessageBlockType::ExecutionSummary: return "execution_summary";
        case AssistantShellMessageBlockType::Confirmation: return "confirmation";
        case AssistantShellMessageBlockType::ArtifactCard: return "artifact_card";
        case AssistantShellMessageBlockType::StatusNotice: return "status_notice";
    }
    return "assistant_response";
}

std::string to_string(AssistantShellSessionMode mode) {
    switch (mode) {
        case AssistantShellSessionMode::Concise: return "concise";
        case AssistantShellSessionMode::Extended: return "extended";
    }
    return "concise";
}

}  // namespace life_orchestrator::app::assistant_shell
