#pragma once

#include <string>

namespace life_orchestrator::ui::assistant_shell {

enum class ComposerSubmitAction { None, Submit, InsertNewline };

class AssistantShellComposerInput {
public:
    static bool CanSubmit(const std::string& text);
    static ComposerSubmitAction ResolveEnterKey(bool shift_pressed);
};

}  // namespace life_orchestrator::ui::assistant_shell
