#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/env.sh
sammler_preset="${1:-dev}"
mapfile -d '' sammler_files < <(find src tests -type f \( -name '*.h' -o -name '*.cpp' \) -print0)
clang-format --dry-run --Werror "${sammler_files[@]}"
cmake --preset "$sammler_preset"
cmake --build --preset "$sammler_preset" --parallel 4
ctest --preset "$sammler_preset"
if [[ "$sammler_preset" != asan ]]; then
  mapfile -d '' sammler_sources < <(find src -name '*.cpp' -print0)
  clang-tidy -p "build/$sammler_preset" "${sammler_sources[@]}" --quiet
fi
git diff --check
