#pragma once
#include <map>
#include <vector>
#include <string>

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
using fs_map = std::map<std::string, std::string>;
using fs_vec = std::vector<fs_map>;
