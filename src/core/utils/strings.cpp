#include "beekeeper/util.hpp"
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <unistd.h>
#include <unordered_map>

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
    std::vector<std::string> suffixes = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};

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

    // Always append suffix
    oss << " " << suffixes[i];

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
    if (s.size() != 36)
        return false;

    for (size_t i = 0; i < 36; ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (s[i] != '-')
                return false;
        }
        else
        {
            if (!std::isxdigit(static_cast<unsigned char>(s[i])))
                return false;
        }
    }

    return true;
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

    // Prepare needles: remove empty needles and parse key=val needles
    struct Needle {
        bool is_kv = false;
        std::string key;
        std::string val;
        std::string raw; // original needle for non-kv
    };

    std::vector<Needle> needles;
    needles.reserve(substrs_to_find.size());
    for (const auto &s : substrs_to_find) {
        if (s.empty()) continue;
        auto pos = s.find('=');
        if (pos != std::string::npos) {
            Needle n;
            n.is_kv = true;
            n.key = s.substr(0, pos);
            n.val = s.substr(pos + 1);
            // strip optional surrounding quotes from needle value
            if (n.val.size() >= 2 &&
                ((n.val.front() == '"' && n.val.back() == '"') ||
                 (n.val.front() == '\'' && n.val.back() == '\''))) {
                n.val = n.val.substr(1, n.val.size() - 2);
            }
            needles.push_back(std::move(n));
        } else {
            Needle n;
            n.is_kv = false;
            n.raw = s;
            needles.push_back(std::move(n));
        }
    }
    if (needles.empty()) return result;

    // Helper lambdas for comparisons
    auto eq_compare = [&](const std::string &a, const std::string &b) -> bool {
        if (case_insensitive) return bk_util::compare_strings_case_insensitive(a, b);
        return a == b;
    };

    auto contains_compare = [&](const std::string &hay, const std::string &needle) -> bool {
        if (needle.empty()) return false;
        if (case_insensitive) {
            // fast path: try sliding-window equality using existing comparator
            size_t nlen = needle.size();
            if (nlen > hay.size()) return false;
            for (size_t p = 0; p + nlen <= hay.size(); ++p) {
                if (bk_util::compare_strings_case_insensitive(hay.substr(p, nlen), needle))
                    return true;
            }
            return false;
        } else {
            return hay.find(needle) != std::string::npos;
        }
    };

    // For each line, tokenize and match needles deterministically
    for (const auto &line : lines) {
        if (line.empty()) continue;

        // Tokenize with existing tokenizer so quotes are respected
        std::vector<std::string> tokens = bk_util::tokenize(line, ' ');

        bool matched_line = false;

        // Build a map of key->value for tokens that contain '=' to speed repeated needle checks
        std::unordered_map<std::string, std::string> kv_map;
        kv_map.reserve(tokens.size());
        for (const auto &tok : tokens) {
            auto eqpos = tok.find('=');
            if (eqpos != std::string::npos && eqpos > 0) {
                std::string k = tok.substr(0, eqpos);
                std::string v = tok.substr(eqpos + 1);
                v = bk_util::trip_quotes(v);
                kv_map.emplace(std::move(k), std::move(v));
            }
        }

        // Check all needles; stop on first match
        for (const auto &n : needles) {
            if (n.is_kv) {
                // Need exact key=value match (key compared literally or case-insensitive)
                auto it = kv_map.find(n.key);
                if (it != kv_map.end()) {
                    if (eq_compare(it->second, n.val)) {
                        matched_line = true;
                        break;
                    }
                } else if (case_insensitive) {
                    // If case-insensitive, try to find a key ignoring case
                    for (const auto &kv : kv_map) {
                        if (bk_util::compare_strings_case_insensitive(kv.first, n.key) &&
                            eq_compare(kv.second, n.val)) {
                            matched_line = true;
                            break;
                        }
                    }
                    if (matched_line) break;
                }
            } else {
                // Plain needle: match against tokens (after trip_quotes) either by equality OR substring
                for (const auto &tok : tokens) {
                    std::string v = tok;
                    // remove surrounding quotes if present
                    if (v.size() >= 2 &&
                        ((v.front() == '"' && v.back() == '"') ||
                         (v.front() == '\'' && v.back() == '\''))) {
                        v = v.substr(1, v.size()-2);
                    }
                    if (eq_compare(v, n.raw) || contains_compare(v, n.raw)) {
                        matched_line = true;
                        break;
                    }
                    // also try with the raw token (to catch things like TYPE="btrfs")
                    if (eq_compare(tok, n.raw) || contains_compare(tok, n.raw)) {
                        matched_line = true;
                        break;
                    }
                }
                if (matched_line) break;
            }
        }

        if (matched_line) {
            result.push_back(line);
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

/**
 * @brief Tokenize a single line into tokens using a delimiter, respecting quotes.
 *
 * This tokenizer splits @p line by @p split_char, but treats quoted sections
 * (single quotes '...' and double quotes "..." ) as grouped text **only when the
 * quote appears as the first character of the token currently being built**.
 *
 * Rules (summary):
 *  - A quote only opens a quoted section when it appears as the first character
 *    of the token (i.e. immediately after a delimiter or at the start of the line).
 *    Quotes that appear later in a token (e.g. after '=') are treated as
 *    ordinary characters and do NOT start a quoted section.
 *
 *  - A closing quote only closes a quoted section if the same type of quote was
 *    previously opened. Mixed quote types inside an opened quoted section are
 *    treated as normal characters.
 *
 *  - Escaped quotes:
 *      * A quote preceded by an odd number of backslashes is considered escaped.
 *        The escaping backslash is removed and the quote character itself is
 *        appended to the token.
 *
 *  - The delimiter (@p split_char) separates tokens only when not inside an
 *    opened quoted section.
 *
 *  - If a quoted section is not closed by the end of the line, everything from
 *    the opening quote to the end of the line remains part of that final token.
 *    A warning is emitted to std::cerr:
 *      "warn: left unclosed quotes in line: <line>"
 *
 * Example (split_char = ' '):
 *  Input:  /dev/mapper/malasdecisiones: LABEL="Malas Decisiones" UUID="22b..."
 *  Output: ["/dev/mapper/malasdecisiones:", "LABEL=\"Malas Decisiones\"", "UUID=\"22b...\"", "TYPE=\"btrfs\""]
 *
 * @param line       Input string to tokenize.
 * @param split_char Delimiter character (commonly space).
 * @return Vector of tokens.
 */
std::vector<std::string>
bk_util::tokenize(const std::string &line, char split_char)
{
    std::vector<std::string> tokens;
    std::string current;
    char quote_char = '\0'; // active quote type, or '\0' if none

    auto count_backslashes_before = [&](size_t pos) -> size_t {
        size_t cnt = 0;
        while (pos > 0 && line[pos - 1] == '\\') {
            ++cnt;
            --pos;
        }
        return cnt;
    };

    // --- First pass: basic tokenization (same as before) ---
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        bool is_quote = (c == '"' || c == '\'');
        size_t backslashes = count_backslashes_before(i);
        bool escaped = (backslashes % 2 == 1);

        if (is_quote && !escaped) {
            // OPENING: only if quote is the leading char of the token (current empty)
            if (quote_char == '\0' && current.empty()) {
                quote_char = c;
                // do not append the opening quote to the token
                continue;
            }

            // CLOSING: only if we are inside a quoted section and types match
            if (quote_char != '\0' && quote_char == c) {
                quote_char = '\0';
                // do not append the closing quote to the token
                continue;
            }

            // Otherwise treat the quote as a normal character (e.g. LABEL="...")
            current += c;
            continue;
        }

        // Escaped quote: remove one escaping backslash if present and append quote
        if (is_quote && escaped) {
            if (!current.empty() && current.back() == '\\') {
                current.pop_back();
            }
            current += c;
            continue;
        }

        // Delimiter outside quotes -> finish token
        if (c == split_char && quote_char == '\0') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            // skip repeated delimiters
            continue;
        }

        // Normal character (including delimiter inside quotes)
        current += c;
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    // DEBUG_LOG("intermediate tokenization: ", bk_util::serialize_vector(tokens));

    // Note: don't emit the old "left unclosed quotes" here yet — we will
    // determine unclosed quotes after the squash pass below, because opening
    // quotes that appear *after* an '=' might have been split into the next token.

    // --- Second pass: squash tokens when an '=' is followed by a quote that was split across tokens ---
    std::vector<std::string> out;
    out.reserve(tokens.size());

    auto has_unescaped_quote = [&](const std::string &s, char q) -> bool {
        // return true if s contains an unescaped q
        for (size_t pos = 0; pos < s.size(); ++pos) {
            if (s[pos] != q) continue;
            // count backslashes immediately before pos
            size_t bs = 0;
            size_t k = pos;
            while (k > 0 && s[k - 1] == '\\') { ++bs; --k; }
            if (bs % 2 == 0) return true; // not escaped
        }
        return false;
    };

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string &tok = tokens[i];

        // If token does not contain '=' we skip squash logic
        size_t eqpos = tok.find('=');
        if (eqpos == std::string::npos) {
            out.push_back(tok);
            continue;
        }

        // Look for an opening quote *after* '=' in the same token (skip spaces)
        size_t p = eqpos + 1;
        while (p < tok.size() && isspace(static_cast<unsigned char>(tok[p]))) ++p;

        bool opening_here = false;
        char opening_quote = '\0';

        if (p < tok.size() && (tok[p] == '"' || tok[p] == '\'')) {
            opening_here = true;
            opening_quote = tok[p];
            // check if the same token already contains a closing unescaped quote later
            size_t close_in_same = tok.find(opening_quote, p + 1);
            if (close_in_same != std::string::npos) {
                // there is a closing quote in the same token -> nothing to squash
                out.push_back(tok);
                continue;
            }
            // otherwise we must merge with following tokens until we find a closing quote
        } else {
            // No quote immediately in this token; but if next token exists and starts
            // with a leading quote (it would, because tokenizer keeps leading quotes
            // when token is empty at quote start), we should also squash starting from here.
            if (i + 1 < tokens.size()) {
                const std::string &nexttok = tokens[i + 1];
                if (!nexttok.empty() && (nexttok[0] == '"' || nexttok[0] == '\'')) {
                    opening_here = true;
                    opening_quote = nexttok[0];
                    // Also check if nexttok contains a closing quote (unescaped) after position 0
                    if (has_unescaped_quote(nexttok.substr(1), opening_quote)) {
                        // close in next token -> merge tok + ' ' + nexttok and continue
                        std::string merged = tok + std::string(1, split_char) + nexttok;
                        out.push_back(std::move(merged));
                        ++i; // skip the next token because included
                        continue;
                    }
                    // otherwise we need to merge further tokens until closing quote found
                } else {
                    // Next token does not start with a quote, nothing to squash
                    out.push_back(tok);
                    continue;
                }
            } else {
                // No next token, nothing to squash
                out.push_back(tok);
                continue;
            }
        }

        // If we reach here, we determined there is an opening quote either in this token
        // after '=' (opening_here==true) and it lacks a closing quote in the same token,
        // or the opening quote was in the next token and didn't close there.
        // Merge tokens from i .. j until we find an unescaped closing opening_quote.
        std::string merged = tok;
        bool closed = false;
        size_t j = i + 1;
        for (; j < tokens.size(); ++j) {
            // append a single split_char to represent the original space(s)
            merged.push_back(split_char);
            merged.append(tokens[j]);

            if (has_unescaped_quote(tokens[j], opening_quote)) {
                closed = true;
                ++j; // point to token after the one we included
                break;
            }
        }

        if (!closed) {
            // no closing quote found until end — emit warning exactly once
            std::cerr << "warn: left unclosed quotes in line: " << line << std::endl;
            // push merged as-is (it includes all remaining tokens)
            out.push_back(std::move(merged));
            // we're done with all tokens
            break;
        } else {
            // closed: push merged and continue after the consumed tokens
            out.push_back(std::move(merged));
            i = j - 1; // advance outer loop to the last consumed token
            continue;
        }
    }

    return out;
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

/**
 * @brief Splits a command_streams object into separate vectors of lines.
 *
 * Takes the stdout and stderr strings from a command_streams struct and
 * splits each by the newline character '\n' using bk_util::tokenize().
 *
 * @param cs The command_streams object containing stdout_str and stderr_str.
 * @return A pair of vectors: first is stdout lines, second is stderr lines.
 */
std::pair<std::vector<std::string>, std::vector<std::string>>
bk_util::split_command_streams_by_lines(const command_streams &cs)
{
    std::vector<std::string> stdout_vec = bk_util::tokenize(cs.stdout_str, '\n');
    std::vector<std::string> stderr_vec = bk_util::tokenize(cs.stderr_str, '\n');
    return {stdout_vec, stderr_vec};
}