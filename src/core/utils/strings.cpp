#include "beekeeper/util.hpp"
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <regex>
#include <unistd.h>

// Case-insensitive string comparison
// Use bk_util::to_lower (assumes it returns a std::string)
bool
bk_util::compare_strings_case_insensitive(const std::string &a, const std::string &b)
{
    return bk_util::to_lower(a) == bk_util::to_lower(b);
}

std::string
bk_util::trim_string (const std::string& str)
{
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    
    return std::string(start, end + 1);
}

// Helper: escape string for safe embedding into a JSON string value.
// Minimal escaping for JSON string values: backslash, quote and control chars.
std::string
bk_util::json_escape (const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    // control character -> \u00XX
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

std::string
bk_util::trip_quotes(const std::string &s)
{
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string
bk_util::quote_if_needed(const std::string &input)
{
    if (input.empty())
        return "\"\"";

    if (input.front() == '"' && input.back() == '"')
        return input;

    return "\"" + input + "\"";
}

// Divide and apply suffix to a byte size number
std::string
bk_util::auto_size_suffix(size_t size_in_bytes)
{
    double size = static_cast<double>(size_in_bytes);
    std::vector<std::string> suffixes = {"", "KiB", "MiB", "GiB", "TiB", "PiB"};

    size_t i = 0;
    while (i + 1 < suffixes.size() && size >= 1024.0) {
        size /= 1024.0;
        ++i;
    }

    // Round to 2 decimal places
    std::ostringstream oss;
    if (std::fabs(size - std::round(size)) < 1e-9) {
        // Integer, drop decimals
        oss << static_cast<int64_t>(std::round(size));
    } else {
        // Up to 2 decimals
        oss << std::fixed << std::setprecision(2) << size;
    }

    if (!suffixes[i].empty()) {
        oss << " " << suffixes[i];
    }

    return oss.str();
}

// Trim helper: remove everything up to and including the first ':' and trim whitespace
std::string
bk_util::trim_config_path_after_colon(const std::string &raw)
{
    if (raw.empty())
        return "";

    // special-case beekeeperman "no config" message
    if (raw.rfind("No configuration found", 0) == 0)
        return "";

    std::string s = raw;
    auto pos = s.find(':');
    if (pos != std::string::npos) {
        s = s.substr(pos + 1);
    }
    // trim leading
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    // trim trailing
    if (!s.empty()) {
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
    }
    return s;
}

std::string
bk_util::serialize_vector(const std::vector<std::string> &vec)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << vec[i];
        if (i + 1 < vec.size())
            oss << ", ";
    }
    oss << "]";
    return oss.str();
}

std::string
bk_util::to_lower(const std::string& str) {
    std::string lower;
    for (char c : str) {
        lower += std::tolower(static_cast<unsigned char>(c));
    }
    return lower;
}

std::string
bk_util::which(const std::string &program)
{
    std::string cmd = bk_util::trim_string(program);

    // Take only the first word up to the first space
    size_t space_pos = cmd.find(' ');
    if (space_pos != std::string::npos) {
        cmd = cmd.substr(0, space_pos);
    }

    const char *pathEnv = std::getenv("PATH");
    if (!pathEnv) return "";

    std::stringstream ss(pathEnv);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        std::string candidate = dir + "/" + cmd;
        if (::access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }

    return "";
}

bool
bk_util::is_uuid(const std::string &s)
{
    static const std::regex uuid_pattern(
        R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})"
    );
    return std::regex_match(s, uuid_pattern);
}

std::string
bk_util::get_second_token (std::string line)
{
    // Shove off the first token
    size_t start = line.find_first_of(" \t");
    if (start == std::string::npos) {
        return "";  // Empty line
    }
    line = line.substr(start);

    // Trim whitespace
    line = bk_util::trim_string(line);
    
    // Find first whitespace after PID to shove off everything else
    size_t end = line.find_first_of(" \t", start);
    if (end == std::string::npos) {
        end = line.length();
    }
    std::string token = line.substr(0, end);

    return token;
}

// Find all lines from 'lines' that contain ANY needle from 'substrs_to_find'.
// If none found, return an empty vector.
//
// If case_insensitive == true, matching is performed using
// bk_util::compare_strings_case_insensitive on each candidate substring slice
// (this avoids allocating/lowering the full haystack). Returned lines are the
// original input lines (not lowercased).
std::vector<std::string>
bk_util::find_lines_matching_substring_in_vector(
    const std::vector<std::string> &lines,
    const std::vector<std::string> &substrs_to_find,
    bool case_insensitive,
    size_t max_coincidence_lines_count)
{
    std::vector<std::string> result;
    if (lines.empty() || substrs_to_find.empty()) return result;

    // Prepare needles: remove empty needles
    std::vector<std::string> needles;
    needles.reserve(substrs_to_find.size());
    for (const auto &s : substrs_to_find) {
        if (!s.empty()) needles.push_back(s);
    }
    if (needles.empty()) return result;

    // Helpers for case-insensitive substring containment:
    // uses existing bk_util::compare_strings_case_insensitive
    auto contains_case_insensitive = [](const std::string &hay, const std::string &needle) -> bool {
        if (needle.size() > hay.size()) return false;
        const size_t nlen = needle.size();
        for (size_t pos = 0; pos + nlen <= hay.size(); ++pos) {
            // compare hay.substr(pos, nlen) vs needle using compare_strings_case_insensitive
            if (bk_util::compare_strings_case_insensitive(hay.substr(pos, nlen), needle))
                return true;
        }
        return false;
    };

    // Scan lines: for each line, if any needle matches, add original line to result.
    for (const auto &line : lines) {
        if (line.empty()) continue;

        bool matched = false;

        if (!case_insensitive) {
            // Fast path: use std::string::find
            for (const auto &needle : needles) {
                if (line.find(needle) != std::string::npos) {
                    matched = true;
                    break;
                }
            }
        } else {
            // Case-insensitive path: use compare_strings_case_insensitive on candidate slices
            for (const auto &needle : needles) {
                if (contains_case_insensitive(line, needle)) {
                    matched = true;
                    break;
                }
            }
        }

        if (matched) {
            result.push_back(line);

            // Early return if we've reached the requested maximum
            if (max_coincidence_lines_count > 0 &&
                result.size() >= max_coincidence_lines_count) {
                return result;
            }
        }
    }

    return result;
}

// Single-string convenience overload: forwards to the vector variant.
std::vector<std::string>
bk_util::find_lines_matching_substring_in_vector(
    const std::vector<std::string> &lines,
    const std::string &substr_to_find,
    bool case_insensitive,
    size_t max_coincidence_lines_count
)
{
    return find_lines_matching_substring_in_vector(
        lines,
        std::vector<std::string>{ substr_to_find },
        case_insensitive,
        max_coincidence_lines_count
    );
}

std::vector<std::string>
bk_util::tokenize(const std::string &line, char split_char)
{
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == '"') {
            // Check if this quote is escaped
            if (i > 0 && line[i-1] == '\\') {
                // Remove the backslash and treat as literal quote
                current.back() = '"';
            } else {
                in_quotes = !in_quotes; // toggle quotes
            }
        }
        else if (c == split_char && !in_quotes) {
            // End of token
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
        else {
            current += c;
        }
    }

    if (!current.empty())
        tokens.push_back(current);

    if (in_quotes) {
        std::cerr << "warn: left unclosed quotes in line: " << line << std::endl;
    }

    return tokens;
}

std::string
bk_util::unescape_proc_mount_field(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size()
            && std::isdigit(static_cast<unsigned char>(s[i+1]))
            && std::isdigit(static_cast<unsigned char>(s[i+2]))
            && std::isdigit(static_cast<unsigned char>(s[i+3])))
        {
            // three octal digits
            int v = (s[i+1]-'0')*64 + (s[i+2]-'0')*8 + (s[i+3]-'0');
            out.push_back(static_cast<char>(v));
            i += 3;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}