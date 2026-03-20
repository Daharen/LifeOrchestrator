#pragma once

#include <string>
#include <vector>

namespace life_orchestrator::app {

struct ActionFormFieldOptionSpec {
    std::string value;
    std::string label;
};

struct ActionFormFieldVisibilityRule {
    std::string controlling_field_id;
    std::string expected_value;
};

struct ActionFormFieldSpec {
    std::string field_id;
    std::string label;
    std::vector<std::string> accepted_flags;
    bool required = false;
    std::string help_text;
    std::string example_value;
    std::string input_kind = "text";
    std::vector<ActionFormFieldOptionSpec> options;
    std::vector<ActionFormFieldVisibilityRule> visibility_rules;
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
