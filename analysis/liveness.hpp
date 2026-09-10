cd ~/THE-OG-TECHUILAGUY/03-compiler/oglang

mkdir -p analysis

cat > analysis/liveness.hpp <<'EOF'
#pragma once

#include "../ir/ir.hpp"
#include <unordered_map>

struct LiveRange {
    int start;
    int end;
};

class LivenessAnalyzer {
public:
    std::unordered_map<ValueId, LiveRange>
    analyze(const IRFunction& function);
};
EOF
