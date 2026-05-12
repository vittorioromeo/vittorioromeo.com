#include "enum_to_string_importstd.hpp"

enum class E {
    V0,
    V1,
    V2,
    V3,
};

volatile int runtime_idx = 0;

int main()
{
    const E val = static_cast<E>(runtime_idx);
    const auto sv = to_enum_string(val);
    return static_cast<int>(sv.size());
}
