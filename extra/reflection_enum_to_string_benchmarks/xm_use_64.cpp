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
    X(V15) \
    X(V16) \
    X(V17) \
    X(V18) \
    X(V19) \
    X(V20) \
    X(V21) \
    X(V22) \
    X(V23) \
    X(V24) \
    X(V25) \
    X(V26) \
    X(V27) \
    X(V28) \
    X(V29) \
    X(V30) \
    X(V31) \
    X(V32) \
    X(V33) \
    X(V34) \
    X(V35) \
    X(V36) \
    X(V37) \
    X(V38) \
    X(V39) \
    X(V40) \
    X(V41) \
    X(V42) \
    X(V43) \
    X(V44) \
    X(V45) \
    X(V46) \
    X(V47) \
    X(V48) \
    X(V49) \
    X(V50) \
    X(V51) \
    X(V52) \
    X(V53) \
    X(V54) \
    X(V55) \
    X(V56) \
    X(V57) \
    X(V58) \
    X(V59) \
    X(V60) \
    X(V61) \
    X(V62) \
    X(V63)

DEFINE_ENUM(E, E_LIST)

volatile int runtime_idx = 0;

int main()
{
    const E val = static_cast<E>(runtime_idx);
    const auto sv = to_string(val);
    return static_cast<int>(sv.size());
}
