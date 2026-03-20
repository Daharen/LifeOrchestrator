#include "ui/artifact_renderer.hpp"

#include <algorithm>
#include <sstream>

namespace life_orchestrator::ui {
namespace {
void append_field_line(std::ostringstream& out, const ArtifactRenderField& field, int depth) {
    out << std::string(static_cast<std::size_t>(depth) * 2, ' ') << field.key;
    if (!field.value.empty()) out << '=' << field.value;
    out << '\n';
    for (const auto& child : field.children) append_field_line(out, child, depth + 1);
}
}

ArtifactRenderModel render_artifact(const app::ArtifactEnvelope& artifact) {
    ArtifactRenderModel model{artifact.artifact_type, artifact.artifact_id, {}};
    std::vector<std::pair<std::string, std::string>> fields(artifact.fields.begin(), artifact.fields.end());
    std::sort(fields.begin(), fields.end());
    for (const auto& [key, value] : fields) model.fields.push_back({key, value, false, {}});

    std::vector<std::pair<std::string, core::StringMap>> nested(artifact.nested_metadata.begin(), artifact.nested_metadata.end());
    std::sort(nested.begin(), nested.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [section, values] : nested) {
        ArtifactRenderField field{section, {}, true, {}};
        std::vector<std::pair<std::string, std::string>> ordered(values.begin(), values.end());
        std::sort(ordered.begin(), ordered.end());
        for (const auto& [key, value] : ordered) field.children.push_back({key, value, false, {}});
        model.fields.push_back(std::move(field));
    }
    return model;
}

std::string render_artifact_as_text(const app::ArtifactEnvelope& artifact) {
    const auto model = render_artifact(artifact);
    std::ostringstream out;
    out << "artifact_type=" << model.artifact_type << '\n';
    out << "artifact_id=" << model.artifact_id << '\n';
    for (const auto& field : model.fields) append_field_line(out, field, 0);
    return out.str();
}

}  // namespace life_orchestrator::ui
