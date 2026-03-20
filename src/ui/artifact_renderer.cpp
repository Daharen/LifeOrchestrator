#include "ui/artifact_renderer.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace life_orchestrator::ui {
namespace {
ArtifactRenderField make_field(const std::string& key,
                               const std::string& label,
                               const app::ArtifactEnvelope& artifact) {
    const auto it = artifact.fields.find(key);
    return {key, label, it == artifact.fields.end() ? std::string{} : it->second, false, {}};
}

void append_field_line(std::ostringstream& out, const ArtifactRenderField& field, int depth) {
    out << std::string(static_cast<std::size_t>(depth) * 2, ' ') << field.key;
    if (!field.value.empty()) out << '=' << field.value;
    out << '\n';
    for (const auto& child : field.children) append_field_line(out, child, depth + 1);
}
}  // namespace

ArtifactRenderModel render_artifact(const app::ArtifactEnvelope& artifact,
                                    const app::ArtifactPresentationSchema& schema) {
    ArtifactRenderModel model{artifact.artifact_type, artifact.artifact_id, schema.display_title, schema.empty_state_text, {}, {}, {}};
    std::unordered_set<std::string> used_keys;
    for (const auto& spec : schema.summary_fields) {
        model.summary_fields.push_back(make_field(spec.field_key, spec.display_label, artifact));
        used_keys.insert(spec.field_key);
    }
    for (const auto& spec : schema.detail_fields) {
        model.detail_fields.push_back(make_field(spec.field_key, spec.display_label, artifact));
        used_keys.insert(spec.field_key);
    }

    for (const auto& group : schema.metadata_groups) {
        ArtifactRenderField render_group{group.group_key, group.display_label, {}, group.collapsible, {}};
        for (const auto& field : group.fields) {
            render_group.children.push_back(make_field(field.field_key, field.display_label, artifact));
            used_keys.insert(field.field_key);
        }
        model.metadata_groups.push_back(std::move(render_group));
    }

    std::vector<std::pair<std::string, std::string>> extras;
    for (const auto& [key, value] : artifact.fields) {
        if (!used_keys.contains(key)) extras.push_back({key, value});
    }
    std::sort(extras.begin(), extras.end());
    for (const auto& [key, value] : extras) model.detail_fields.push_back({key, key, value, false, {}});

    std::vector<std::pair<std::string, core::StringMap>> nested(artifact.nested_metadata.begin(), artifact.nested_metadata.end());
    std::sort(nested.begin(), nested.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [section, values] : nested) {
        ArtifactRenderField group{section, section, {}, true, {}};
        std::vector<std::pair<std::string, std::string>> ordered(values.begin(), values.end());
        std::sort(ordered.begin(), ordered.end());
        for (const auto& [key, value] : ordered) group.children.push_back({key, key, value, false, {}});
        model.metadata_groups.push_back(std::move(group));
    }

    return model;
}

std::string render_artifact_as_text(const app::ArtifactEnvelope& artifact,
                                    const app::ArtifactPresentationSchema& schema) {
    const auto model = render_artifact(artifact, schema);
    std::ostringstream out;
    out << "artifact_type=" << model.artifact_type << '\n';
    out << "artifact_id=" << model.artifact_id << '\n';
    for (const auto& field : model.summary_fields) append_field_line(out, field, 0);
    for (const auto& field : model.detail_fields) append_field_line(out, field, 0);
    for (const auto& field : model.metadata_groups) append_field_line(out, field, 0);
    return out.str();
}

}  // namespace life_orchestrator::ui
