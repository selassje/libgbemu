export module gbemu:hard_assert;

import std;

namespace gbemu {

void
hardAssert(bool condition, // NOLINT(misc-use-internal-linkage)
           std::string_view message);

}
