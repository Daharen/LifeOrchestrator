#pragma once

#include <string>
#include <vector>

namespace life_orchestrator::app {

struct ActionFormFieldSpec {
    std::string field_id;
    std::string label;
    std::vector<std::string> accepted_flags;
    bool required = false;
    std::string help_text;
    std::string example_value;
};

struct ActionFormSpec {
    std::string action_id;
    std::string display_label;
    std::string canonical_command_target;
    std::vector<ActionFormFieldSpec> input_fields;
    std::string example_payload;
    std::vector<std::string> refresh_targets;
};

}  // namespace life_orchestrator::app
