#!/bin/bash
set -e
cd "$(dirname "$0")"

cat > xmcc_include_only.cpp <<'EOF'
#include "xmacro_enum_cc.hpp"
int main() {}
EOF

gen_use() {
    local n=$1
    local out="xmcc_use_${n}.cpp"
    {
        echo '#include "xmacro_enum_cc.hpp"'
        echo ''
        printf '#define E_LIST(X) \\\n'
        for ((i = 0; i < n - 1; i++)); do
            printf '    X(V%d) \\\n' "$i"
        done
        printf '    X(V%d)\n' "$((n - 1))"
        echo ''
        echo 'DEFINE_ENUM_CC(E, E_LIST)'
        echo ''
        echo 'volatile int runtime_idx = 0;'
        echo ''
        echo 'int main()'
        echo '{'
        echo '    const E val = static_cast<E>(runtime_idx);'
        echo '    const char* s = to_cstr(val);'
        echo '    // measure pointer use without pulling in <cstring>'
        echo '    int len = 0;'
        echo '    while (s[len] != 0) ++len;'
        echo '    return len;'
        echo '}'
    } > "$out"
}

for n in 4 16 64 256 1024; do
    gen_use $n
done

echo "Generated:"
ls xmcc_*.cpp
