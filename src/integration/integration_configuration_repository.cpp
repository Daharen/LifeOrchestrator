#include "integration/integration_configuration_repository.hpp"

#include <algorithm>
#include <fstream>

namespace life_orchestrator::integration {
namespace {

std::filesystem::path config_file(const std::filesystem::path& root) {
    return root / "memory/integration_configuration/records.ndjson";
}

std::filesystem::path manifest_file(const std::filesystem::path& root) {
    return root / "memory/integration_configuration/manifest.json";
}

void ensure_parent(const std::filesystem::path& path) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
}

std::string join(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        out += values[i];
    }
    return out;
}

std::vector<std::string> split(const std::string& value) {
    std::vector<std::string> out;
    std::string token;
    for (char ch : value) {
        if (ch == ',') {
            out.push_back(token);
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    if (!token.empty()) {
        out.push_back(token);
    }
    return out;
}

std::string encode_map(const life_orchestrator::core::StringMap& values) {
    std::vector<std::pair<std::string, std::string>> ordered(values.begin(), values.end());
    std::sort(ordered.begin(), ordered.end());
    std::string out;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        out += ordered[i].first + "=" + ordered[i].second;
    }
    return out;
}

life_orchestrator::core::StringMap decode_map(const std::string& value) {
    life_orchestrator::core::StringMap out;
    if (value.empty()) {
        return out;
    }
    std::string token;
    for (char ch : value) {
        if (ch == ',') {
            const auto pos = token.find('=');
            if (pos != std::string::npos) {
                out[token.substr(0, pos)] = token.substr(pos + 1);
            }
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    if (!token.empty()) {
        const auto pos = token.find('=');
        if (pos != std::string::npos) {
            out[token.substr(0, pos)] = token.substr(pos + 1);
        }
    }
    return out;
}

}  // namespace

IntegrationConfigurationRepository::IntegrationConfigurationRepository(std::filesystem::path data_root)
    : data_root_(std::move(data_root)) {}

core::MemoryResult IntegrationConfigurationRepository::upsert(
    const core::IntegrationConfigurationRecord& record) {
    by_id_[record.integration_config_id] = record;
    ensure_parent(config_file(data_root_));
    std::ofstream out(config_file(data_root_), std::ios::app);
    if (!out.is_open()) {
        return {false, "unable to open integration config file"};
    }

    out << "integration_config_id=" << record.integration_config_id << ';'
        << "integration_id=" << record.integration_id << ';'
        << "display_name=" << record.display_name << ';'
        << "enabled=" << (record.enabled ? "1" : "0") << ';'
        << "status=" << core::to_string(record.status) << ';'
        << "capability_visibility=" << join(record.capability_visibility) << ';'
        << "credential_storage_mode=" << core::to_string(record.credential_storage_mode) << ';'
        << "credential_reference=" << record.credential_reference << ';'
        << "non_secret_settings=" << encode_map(record.non_secret_settings) << ';'
        << "created_at=" << record.created_at << ';'
        << "updated_at=" << record.updated_at << ';'
        << "version=" << record.version << '\n';
    return static_cast<bool>(out) ? core::MemoryResult{true, "ok"}
                                 : core::MemoryResult{false, "write failed"};
}

core::MemoryResult IntegrationConfigurationRepository::load() {
    by_id_.clear();
    std::ifstream in(config_file(data_root_));
    if (!in.is_open()) {
        return {true, "ok"};
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::unordered_map<std::string, std::string> fields;
        std::string token;
        for (char ch : line) {
            if (ch == ';') {
                const auto pos = token.find('=');
                if (pos == std::string::npos) {
                    return {false, "malformed integration configuration line"};
                }
                fields[token.substr(0, pos)] = token.substr(pos + 1);
                token.clear();
            } else {
                token.push_back(ch);
            }
        }
        if (!token.empty()) {
            const auto pos = token.find('=');
            if (pos == std::string::npos) {
                return {false, "malformed integration configuration line"};
            }
            fields[token.substr(0, pos)] = token.substr(pos + 1);
        }

        core::IntegrationStatus status = core::IntegrationStatus::Unknown;
        if (fields["status"] == "Disabled") status = core::IntegrationStatus::Disabled;
        if (fields["status"] == "Enabled") status = core::IntegrationStatus::Enabled;
        if (fields["status"] == "Error") status = core::IntegrationStatus::Error;

        core::CredentialStorageMode storage_mode = core::CredentialStorageMode::Unset;
        if (fields["credential_storage_mode"] == "InlinePlaceholderOnly") {
            storage_mode = core::CredentialStorageMode::InlinePlaceholderOnly;
        }
        if (fields["credential_storage_mode"] == "ExternalSecretReference") {
            storage_mode = core::CredentialStorageMode::ExternalSecretReference;
        }

        core::IntegrationConfigurationRecord record{.integration_config_id = fields["integration_config_id"],
                                                    .integration_id = fields["integration_id"],
                                                    .display_name = fields["display_name"],
                                                    .enabled = fields["enabled"] == "1",
                                                    .status = status,
                                                    .capability_visibility = split(fields["capability_visibility"]),
                                                    .connection_diagnostics = {},
                                                    .credential_storage_mode = storage_mode,
                                                    .credential_reference = fields["credential_reference"],
                                                    .non_secret_settings = decode_map(fields["non_secret_settings"]),
                                                    .created_at = fields["created_at"],
                                                    .updated_at = fields["updated_at"],
                                                    .version = static_cast<core::MemoryVersion>(
                                                        std::stoull(fields["version"]))};
        by_id_[record.integration_config_id] = record;
    }

    return {true, "ok"};
}

core::MemoryResult IntegrationConfigurationRepository::persist_manifest() const {
    ensure_parent(manifest_file(data_root_));
    std::ofstream out(manifest_file(data_root_));
    if (!out.is_open()) {
        return {false, "unable to open integration manifest"};
    }
    out << "{\n"
        << "  \"schema_version\": \"sprint2-v1\",\n"
        << "  \"record_count\": " << by_id_.size() << '\n'
        << "}\n";
    return static_cast<bool>(out) ? core::MemoryResult{true, "ok"}
                                 : core::MemoryResult{false, "manifest write failed"};
}

std::optional<core::IntegrationConfigurationRecord> IntegrationConfigurationRepository::get_by_id(
    const core::IntegrationConfigId& id) const {
    auto it = by_id_.find(id);
    if (it == by_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<core::IntegrationConfigurationRecord> IntegrationConfigurationRepository::list_all() const {
    std::vector<core::IntegrationConfigurationRecord> records;
    records.reserve(by_id_.size());
    for (const auto& [_, value] : by_id_) {
        records.push_back(value);
    }
    std::sort(records.begin(), records.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.integration_config_id < rhs.integration_config_id; });
    return records;
}

}  // namespace life_orchestrator::integration
