#include "beekeeper/util.hpp"

#include <unordered_set>

fs_vec
bk_util::subtract_vectors_of_maps(const fs_vec &A,
                                      const fs_vec &B,
                                      const std::string &key)
{
    fs_vec result;
    if (A.empty()) return result;
    if (B.empty()) return A;

    // Build set of key values from B for fast lookup
    std::unordered_set<std::string> bkeys;
    bkeys.reserve(B.size()*2);
    for (const auto &m : B) {
        auto it = m.find(key);
        if (it != m.end()) bkeys.insert(it->second);
    }

    // Keep items in A if key not in bkeys
    for (const auto &m : A) {
        auto it = m.find(key);
        if (it == m.end()) {
            result.push_back(m); // no key -> keep (defensive)
        } else {
            if (bkeys.find(it->second) == bkeys.end())
                result.push_back(m);
        }
    }
    return result;
}
