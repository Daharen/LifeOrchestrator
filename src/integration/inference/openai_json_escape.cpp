#include "integration/inference/openai_json_escape.h"

#include <cctype>
#include <sstream>

namespace life_orchestrator::integration::inference {
namespace {
std::size_t skip_ws(const std::string& json, std::size_t pos) {
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    return pos;
}

std::optional<std::string> parse_json_string(const std::string& json, std::size_t start, std::size_t* end_pos = nullptr) {
    if (start >= json.size() || json[start] != '"') return std::nullopt;
    std::ostringstream out;
    for (std::size_t i = start + 1; i < json.size(); ++i) {
        const auto ch = json[i];
        if (ch == '\\') {
            if (i + 1 >= json.size()) return std::nullopt;
            const auto esc = json[++i];
            switch (esc) {
                case '"': out << '"'; break;
                case '\\': out << '\\'; break;
                case '/': out << '/'; break;
                case 'b': out << '\b'; break;
                case 'f': out << '\f'; break;
                case 'n': out << '\n'; break;
                case 'r': out << '\r'; break;
                case 't': out << '\t'; break;
                default: out << esc; break;
            }
            continue;
        }
        if (ch == '"') {
            if (end_pos) *end_pos = i + 1;
            return out.str();
        }
        out << ch;
    }
    return std::nullopt;
}

std::optional<std::size_t> find_key(const std::string& json, const std::string& key) {
    const auto token = std::string{"\""} + key + "\"";
    const auto key_pos = json.find(token);
    if (key_pos == std::string::npos) return std::nullopt;
    const auto colon = json.find(':', key_pos + token.size());
    if (colon == std::string::npos) return std::nullopt;
    return skip_ws(json, colon + 1);
}

std::optional<std::string> extract_balanced_raw(const std::string& json, std::size_t start) {
    if (start >= json.size()) return std::nullopt;
    if (json[start] == '"') {
        std::size_t end = 0;
        auto parsed = parse_json_string(json, start, &end);
        if (!parsed) return std::nullopt;
        return json.substr(start, end - start);
    }

    if (json[start] != '{' && json[start] != '[') {
        std::size_t end = start;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']') ++end;
        return json.substr(start, end - start);
    }

    const char open = json[start];
    const char close = open == '{' ? '}' : ']';
    int depth = 0;
    bool in_string = false;
    for (std::size_t i = start; i < json.size(); ++i) {
        const auto ch = json[i];
        if (ch == '"' && (i == 0 || json[i - 1] != '\\')) in_string = !in_string;
        if (in_string) continue;
        if (ch == open) ++depth;
        if (ch == close) {
            --depth;
            if (depth == 0) return json.substr(start, i - start + 1);
        }
    }
    return std::nullopt;
}
}  // namespace

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const auto ch : value) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    return out.str();
}

std::optional<std::string> json_extract_string_field(const std::string& json, const std::string& key) {
    const auto pos = find_key(json, key);
    if (!pos.has_value() || *pos >= json.size() || json[*pos] != '"') return std::nullopt;
    return parse_json_string(json, *pos);
}

std::optional<std::string> json_extract_raw_field(const std::string& json, const std::string& key) {
    const auto pos = find_key(json, key);
    if (!pos.has_value()) return std::nullopt;
    return extract_balanced_raw(json, *pos);
}

}  // namespace life_orchestrator::integration::inference
