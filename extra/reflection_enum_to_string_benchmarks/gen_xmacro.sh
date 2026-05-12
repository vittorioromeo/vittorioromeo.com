#!/bin/bash
set -e
cd "$(dirname "$0")"

cat > xm_include_only.cpp <<'EOF'
#include "xmacro_enum.hpp"
int main() {}
EOF

gen_use() {
    local n=$1
    local out="xm_use_${n}.cpp"
    {
        echo '#include "xmacro_enum.hpp"'
        echo ''
        printf '#define E_LIST(X) \\\n'
        for ((i = 0; i < n - 1; i++)); do
            printf '    X(V%d) \\\n' "$i"
        done
        printf '    X(V%d)\n' "$((n - 1))"
        echo ''
        echo 'DEFINE_ENUM(E, E_LIST)'
        echo ''
        echo 'volatile int runtime_idx = 0;'
        echo ''
        echo 'int main()'
        echo '{'
        echo '    const E val = static_cast<E>(runtime_idx);'
        echo '    const auto sv = to_string(val);'
        echo '    return static_cast<int>(sv.size());'
        echo '}'
    } > "$out"
}

for n in 4 16 64 256 1024; do
    gen_use $n
done

echo "Generated:"
ls xm_*.cpp
