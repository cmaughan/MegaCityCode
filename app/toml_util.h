#pragma once
// Internal helpers shared by app_config.cpp and render_test.cpp.
// All functions are in namespace spectre::toml.

#include <spectre/string_util.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace spectre::toml
{

using spectre::trim;

inline std::string unquote(std::string value)
{
    value = trim(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return value.substr(1, value.size() - 2);
    return value;
}

// Returns true when value holds a syntactically complete [...] literal
// (balanced brackets, all strings closed, no trailing backslash).
inline bool is_complete_array_literal(const std::string& value)
{
    bool in_string = false;
    bool escaping = false;
    int depth = 0;
    bool saw_open = false;

    for (char ch : value)
    {
        if (escaping)
        {
            escaping = false;
            continue;
        }
        if (ch == '\\' && in_string)
        {
            escaping = true;
            continue;
        }
        if (ch == '"')
        {
            in_string = !in_string;
            continue;
        }
        if (in_string)
            continue;
        if (ch == '[')
        {
            saw_open = true;
            ++depth;
        }
        else if (ch == ']')
        {
            --depth;
        }
    }

    return saw_open && depth == 0 && !in_string && !escaping;
}

// Parses a TOML-like string array literal: ["a", "b", "c"].
// Handles quoted items with backslash escapes.  Items outside quotes are trimmed.
inline std::vector<std::string> parse_string_array(const std::string& value)
{
    std::vector<std::string> items;
    std::string trimmed = trim(value);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
        return items;

    std::string current;
    bool in_string = false;
    bool escaping = false;
    for (size_t i = 1; i + 1 < trimmed.size(); ++i)
    {
        char ch = trimmed[i];
        if (escaping)
        {
            current.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\')
        {
            escaping = true;
            continue;
        }
        if (ch == '"')
        {
            in_string = !in_string;
            continue;
        }
        if (!in_string && ch == ',')
        {
            std::string item = trim(current);
            if (!item.empty())
                items.push_back(item);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }

    std::string item = trim(current);
    if (!item.empty())
        items.push_back(item);

    return items;
}

inline int parse_int(const std::string& value, int fallback)
{
    try
    {
        return std::stoi(value);
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

inline double parse_double(const std::string& value, double fallback)
{
    try
    {
        return std::stod(value);
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

inline bool parse_bool(const std::string& value, bool fallback)
{
    std::string normalized = trim(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "true")
        return true;
    if (normalized == "false")
        return false;
    return fallback;
}

// Escapes a string for safe embedding inside a JSON double-quoted value.
inline std::string json_escape_string(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char ch : s)
    {
        switch (ch)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

} // namespace spectre::toml
