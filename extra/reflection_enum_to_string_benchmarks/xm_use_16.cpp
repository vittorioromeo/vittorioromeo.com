#include "xmacro_enum.hpp"

#define E_LIST(X) \
    X(V0) \
    X(V1) \
    X(V2) \
    X(V3) \
    X(V4) \
    X(V5) \
    X(V6) \
    X(V7) \
    X(V8) \
    X(V9) \
    X(V10) \
    X(V11) \
    X(V12) \
    X(V13) \
    X(V14) \
    X(V15)

DEFINE_ENUM(E, E_LIST)

volatile int runtime_idx = 0;

int main()
{
    const E val = static_cast<E>(runtime_idx);
    const auto sv = to_string(val);
    return static_cast<int>(sv.size());
}
