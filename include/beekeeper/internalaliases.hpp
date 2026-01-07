#pragma once
#include <unordered_map>
#include <string>
#include <vector>

// forward declarations
namespace beekeeper {
    namespace management {
        constexpr bool __dummy__ = true;
    }
    namespace __util__ {
        constexpr bool __dummy__ = true;
    }
}

// aliases
namespace bk_mgmt = beekeeper::management;
namespace bk_util = beekeeper::__util__;

// define a dummy type inside a dummy namespace
namespace beekeeper {
    namespace _internalaliases_dummy {
        struct anchor {};
        constexpr bool dummy = bk_mgmt::__dummy__;
        constexpr bool dummy_ = bk_util::__dummy__;
    }
}

// Type aliases
struct fs_info {
    std::string label;
    std::string status;
    std::string devname;
    std::string config; // config file path
    bool compressing; // is transparent compression currently running for it?
    bool autostart; // is autostart enabled? 
};

using fs_map = std::unordered_map<std::string, fs_info>;

struct fs_diff {
    fs_map newly_added;
    std::vector<std::string> just_removed;
    fs_map just_changed;
};