#include "enum_to_string.hpp"

enum class E {
    V0,
    V1,
    V2,
    V3,
    V4,
    V5,
    V6,
    V7,
    V8,
    V9,
    V10,
    V11,
    V12,
    V13,
    V14,
    V15,
};

volatile int runtime_idx = 0;

int main()
{
    const E val = static_cast<E>(runtime_idx);
    const auto sv = to_enum_string(val);
    return static_cast<int>(sv.size());
}
