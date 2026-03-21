#include "ui/assistant_shell/assistant_shell_composer_input.h"

#include <algorithm>
#include <cctype>

namespace life_orchestrator::ui::assistant_shell {

bool AssistantShellComposerInput::CanSubmit(const std::string& text) {
    return std::any_of(text.begin(), text.end(), [](unsigned char ch) { return !std::isspace(ch); });
}

ComposerSubmitAction AssistantShellComposerInput::ResolveEnterKey(bool shift_pressed) {
    return shift_pressed ? ComposerSubmitAction::InsertNewline : ComposerSubmitAction::Submit;
}

}  // namespace life_orchestrator::ui::assistant_shell
