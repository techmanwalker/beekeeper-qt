#include "beekeeper/beesdmgmt.hpp"

#include <fstream>

namespace beekeeper::management::configfile {

// Read UUID-like lines from an arbitrary config file.
// Keeps the same behaviour as previous list_uuids: trims lines, ignores empty lines,
// and only returns tokens that satisfy bk_util::is_uuid().
std::vector<std::string>
list_uuids(const std::string &config_file)
{
    std::vector<std::string> uuids_found;

    // Read all lines from config file
    std::vector<std::string> lines = bk_util::read_lines_from_file(config_file);

    for (const auto &line : lines) {
        // Tokenize and check if the first token is a valid UUID
        std::vector<std::string> tokens = bk_util::tokenize(line);
        if (!tokens.empty() && bk_util::is_uuid(tokens[0])) {
            uuids_found.push_back(tokens[0]);
        }
    }

    return uuids_found;
}

// Check whether ss is present in the given config file.
bool
is_present(const std::string &config_file, const std::string &ss)
{
    // Case-insensitive, stop after first match
    return !fetch(config_file, ss, true, 1).empty();
}

// Remove all occurrences (case-insensitive) of s from config_file.
// Rewrites the file and restores world-readable perms.
void
remove_line_matching_substring(const std::string &config_file, const std::string &s)
{
    if (s.empty())
        return;

    std::string uuid_lower = bk_util::to_lower(bk_util::trim_string(s));

    // Read all lines
    std::vector<std::string> lines = bk_util::read_lines_from_file(config_file);

    // Rewrite excluding matching lines
    std::ofstream outfile(config_file, std::ios::trunc);
    if (!outfile.is_open())
        return;

    for (const auto &l : lines) {
        if (bk_util::to_lower(bk_util::trim_string(l)) == uuid_lower)
            continue; // skip
        outfile << l << "\n";
    }
    outfile.close();

    bk_util::make_file_world_readable(config_file);
}

std::vector<std::string> fetch (
    const std::string &config_file,
    const std::string &substr_to_find,
    bool case_insensitive,
    size_t max_coincidence_lines_count)
{
    return bk_util::find_lines_matching_substring_in_file(
    config_file, substr_to_find, case_insensitive, max_coincidence_lines_count);
}

} // namespace beekeeper::management::configfile