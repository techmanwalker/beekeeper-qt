#include "beekeeper/internalaliases.hpp"
#include "beekeeper/util.hpp"
#include <algorithm>

const fs_info *
bk_util::retrieve_filesystem_info_from_a_list(const fs_map &haystack, const std::string &needle)
{
    auto it = haystack.find(needle);
    if (it == haystack.end())
        return nullptr;

    return &it->second; // immutable, no copy
}

/* NOTE FOR THE DEVS: you must ensure that the next three function's
* return vectors NEVER EVER contain the same UUID across their return
* values.
*
* For example:
*
* Good:
* - added: xxxxx
* - added: yyyyy
* - changed: zzzzz
* - removed: wwwww
*
* DANGEROUS, DON'T ALLOW THIS TO HAPPEN
* - added: xxxxx
- - added: yyyyy
* - changed: xxxxx <- also in "added"
* - changed: yyyyy <- also in "added" and "removed"
* - removed: zzzzz
* - removed: yyyyy
* 
* The same UUID contained within multiple function returns will
* cause table refresh issues in the GUI and break functionality.
* The above "Dangerous" must NEVER happen.
*/

/**
* @brief Return a new fs_vec containing only the new filesystem
* entities.
*
* @return A fs_vec containing filesystem entities from fresh_list
* that don't exist in the base filesystem list.
*
* @param snapshot Your latest filesystem list snapshot, prior refresh.
* @param fresh_list Your new filesystem list snapshot you want to
* compare your last one against; whatever is contained here that
* isn't present in the base fs_vec will be added to this function's
* return.
*/
fs_map
bk_util::list_of_newly_added_filesystems (const std::vector<std::string> &snapshot, const fs_map &fresh_list) 
{
    fs_map newly_added;
    newly_added.reserve(fresh_list.size());

    for (const auto &[uuid, info] : fresh_list) { // don't copy just yet
        if (std::find(snapshot.begin(), snapshot.end(), uuid) == snapshot.end()) {
            newly_added.emplace(uuid, info); // copy if not present
        }

        // if it does exist, do nothing
    }

    return newly_added;
}

/**
* @brief Return a new fs_vec containing the filesystem entities
* that have been removed from the last snapshot.
*
* @return A fs_vec containing filesystem entities from fresh_list
* that don't exist in the newer filesystem list but do exist in
* the base list. Note that this must NOT include newly added
* filesystems.
*
* @param base Your latest filesystem list snapshot, prior refresh.
* @param fresh_list Your new filesystem list snapshot you want to
* compare your last one against; whatever is contained in the base
* list that isn't present in the new fs_vec will be added to this
* function's return.
*/
std::vector<std::string>
bk_util::list_of_just_removed_filesystems (const std::vector<std::string> &snapshot, const fs_map &fresh_list) 
{
    std::vector<std::string> just_removed;
    just_removed.reserve(fresh_list.size());

    for (const std::string &uuid : snapshot) {
        if (fresh_list.find(uuid) == fresh_list.end()) {
            just_removed.emplace_back(uuid);

            // same as above: if it wasn't removed, do nothing
        }
    }

    return just_removed;
}


/**
* @brief Return a new fs_vec which contains all filesystem entities
* that still exist in both snapshot and new list and that have at
* least one of its data values changed.
*
* @return A fs_vec containing all filesystem entities that exist in
* both the snapshot and the new list and have at least one of its data
* values changed, comparing everything but the UUIDs since that's the
* reference key and so those comparisons would always be true.
*/
fs_map
bk_util::list_of_filesystems_that_still_exist_and_were_changed (const fs_map &snapshot, const fs_map &fresh_list)
{
    fs_map just_changed;
    just_changed.reserve(fresh_list.size());

    for (const auto &[uuid, info] : fresh_list) {
        const auto prolly_existed_and_changed = snapshot.find(uuid);

        // if it DOES exist in both new and snapshot
        if (prolly_existed_and_changed != snapshot.end()) {
            // if it changed in something, add it
            if (
                info.label != prolly_existed_and_changed->second.label
            ||  info.status != prolly_existed_and_changed->second.status
            ||  info.devname != prolly_existed_and_changed->second.devname
            ) {
                just_changed.emplace(uuid, info);
            }
        }
    }

    return just_changed;
}

fs_diff
bk_util::difference_between_two_fs_maps (
    const fs_map &snapshot,
    const fs_map &fresh_list
)
{
    fs_diff differences;

    std::vector<std::string> snapshot_uuids;
    for (const auto &[uuid, info] : snapshot) {
        snapshot_uuids.emplace_back(uuid);
    }

    differences.newly_added = list_of_newly_added_filesystems(snapshot_uuids, fresh_list);
    differences.just_removed = list_of_just_removed_filesystems(snapshot_uuids, fresh_list);
    differences.just_changed = list_of_filesystems_that_still_exist_and_were_changed(snapshot, fresh_list);

    return differences;
}