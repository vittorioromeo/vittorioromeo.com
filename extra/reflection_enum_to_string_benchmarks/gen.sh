#!/bin/bash
set -e
cd "$(dirname "$0")"

gen_enum_use() {
    local n=$1
    local out="enum_use_${n}.cpp"
    {
        echo '#include "enum_to_string.hpp"'
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
        echo '    const auto sv = to_enum_string(val);'
        echo '    return static_cast<int>(sv.size());'
        echo '}'
    } > "$out"
}

for n in 4 16 64 256 1024; do
    gen_enum_use $n
done

echo "Generated:"
ls enum_use_*.cpp
