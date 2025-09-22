#include "beekeeper/util.hpp"
#include <unistd.h>

bool
bk_util::is_root ()
{
    return geteuid() == 0;
}