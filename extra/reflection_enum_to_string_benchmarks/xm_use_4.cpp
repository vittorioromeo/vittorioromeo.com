#include "xmacro_enum.hpp"

#define E_LIST(X) \
    X(V0) \
    X(V1) \
    X(V2) \
    X(V3)

DEFINE_ENUM(E, E_LIST)

volatile int runtime_idx = 0;

int main()
{
    const E val = static_cast<E>(runtime_idx);
    const auto sv = to_string(val);
    return static_cast<int>(sv.size());
}
