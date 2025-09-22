#pragma once
#include <string>

namespace beekeeper::management::transparentcompression {

bool is_enabled_for(const std::string &uuid);
bool start(const std::string &uuid);
bool pause(const std::string &uuid);
void add_uuid(const std::string &uuid, const std::string &algorithm, int level);
void remove_uuid(const std::string &uuid);
bool is_running(const std::string &uuid);
bool is_not_running_for_at_least_one_mountpoint_of(const std::string &uuid);
std::pair<std::string, std::string>
get_current_compression_level(const std::string &mountpoint_or_uuid);

} // namespace bk_mgmt::transparentcompression