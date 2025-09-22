#include "beekeeper/util.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

bool
bk_util::file_exists (const std::string& path)
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool
bk_util::file_readable (const std::string& path)
{
    if (access(path.c_str(), R_OK) != 0) {
        std::cerr << "File access error (" << path << "): " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief Read all non-empty, trimmed lines from a file.
 *
 * This function opens a file specified by `path` and reads it line by line.
 * Each line is trimmed of leading and trailing whitespace and added to the
 * returned vector if it is not empty.
 *
 * @note If the file does not exist, cannot be opened, or is empty, the
 *       function returns an empty vector. No exceptions are thrown.
 *       Use bk_util::file_readable() if you want to enforce existence
 *       or permissions checks before reading.
 *
 * @param path Path to the file to read.
 * @return A vector of non-empty, trimmed lines from the file. Returns an
 *         empty vector if the file cannot be read or is empty.
 */
std::vector<std::string>
bk_util::read_lines_from_file(const std::string &path)
{
    std::vector<std::string> lines;

    if (!std::filesystem::exists(path))
        return lines;

    std::ifstream infile(path);
    std::string line;
    while (std::getline(infile, line)) {
        line = bk_util::trim_string(line);
        if (!line.empty())
            lines.push_back(line);
    }
    return lines;
}


void
bk_util::make_file_world_readable(const std::string &path)
{
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write |
        std::filesystem::perms::group_read |
        std::filesystem::perms::others_read,
        std::filesystem::perm_options::add
    );
}

// Wrapper: read file into lines and delegate to the in-memory matcher
std::vector<std::string>
bk_util::find_lines_matching_substring_in_file(
    const std::string &path,
    const std::vector<std::string> &substrs_to_match,
    bool case_insensitive,
    size_t max_coincidence_lines_count)
{
    // Read all lines from the file into a vector
    std::vector<std::string> file_lines = bk_util::read_lines_from_file(path);

    // Delegate the actual matching to the in-memory function, passing the max
    return bk_util::find_lines_matching_substring_in_vector(
        file_lines,
        substrs_to_match,
        case_insensitive,
        max_coincidence_lines_count
    );
}

// Single-string overload: wrap the needle into a one-element vector and delegate.
std::vector<std::string>
bk_util::find_lines_matching_substring_in_file(
    const std::string &path,
    const std::string &substr_to_match,
    bool case_insensitive,
    size_t max_coincidence_lines_count)
{
    return bk_util::find_lines_matching_substring_in_file(
        path,
        std::vector<std::string>{ substr_to_match },
        case_insensitive,
        max_coincidence_lines_count
    );
}