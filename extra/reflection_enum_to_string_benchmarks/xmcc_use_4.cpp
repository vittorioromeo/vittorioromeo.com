#include "xmacro_enum_cc.hpp"

#define E_LIST(X) \
    X(V0) \
    X(V1) \
    X(V2) \
    X(V3)

DEFINE_ENUM_CC(E, E_LIST)

volatile int runtime_idx = 0;

int main()
{
    const E val = static_cast<E>(runtime_idx);
    const char* s = to_cstr(val);
    // measure pointer use without pulling in <cstring>
    int len = 0;
    while (s[len] != 0) ++len;
    return len;
}
