#pragma once

#include "app/app_support/action_form_spec.hpp"

#include <optional>
#include <string>
#include <vector>

namespace life_orchestrator::app {

const std::vector<ActionFormSpec>& list_action_form_specs();
std::vector<std::string> list_action_form_ids();
std::optional<ActionFormSpec> find_action_form_spec_by_id(const std::string& action_id);
std::optional<ActionFormSpec> find_action_form_spec_by_command_target(const std::string& command_target);

}  // namespace life_orchestrator::app
