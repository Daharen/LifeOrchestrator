#pragma once

#include <optional>
#include <string>

namespace life_orchestrator::integration::inference {

std::string json_escape(const std::string& value);
std::optional<std::string> json_extract_string_field(const std::string& json, const std::string& key);
std::optional<std::string> json_extract_raw_field(const std::string& json, const std::string& key);

}  // namespace life_orchestrator::integration::inference
