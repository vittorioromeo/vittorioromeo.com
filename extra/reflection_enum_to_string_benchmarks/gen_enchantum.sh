#!/bin/bash
set -e
cd "$(dirname "$0")"

gen_baseline_include() {
    cat > ench_include_only.cpp <<'EOF'
#include <enchantum/enchantum.hpp>
int main() {}
EOF
}

gen_use() {
    local n=$1
    local out="ench_use_${n}.cpp"
    {
        echo '#include <enchantum/enchantum.hpp>'
        echo ''
        echo 'enum class E {'
        for ((i = 0; i < n; i++)); do
            echo "    V${i},"
        done
        echo '};'
        echo ''
        echo 'volatile int runtime_idx = 0;'
        echo ''
        echo 'int main()'
        echo '{'
        echo '    const E val = static_cast<E>(runtime_idx);'
        echo '    const auto sv = enchantum::to_string(val);'
        echo '    return static_cast<int>(sv.size());'
        echo '}'
    } > "$out"
}

gen_baseline_include
for n in 4 16 64 256 1024; do
    gen_use $n
done

echo "Generated:"
ls ench_*.cpp
