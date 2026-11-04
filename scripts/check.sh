#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/env.sh
collection_preset="${1:-dev}"
collection_files=()
while IFS= read -r -d '' collection_file; do
  collection_files+=("$collection_file")
done < <(find src tests -type f \( -name '*.h' -o -name '*.cpp' \) -print0)
clang-format --dry-run --Werror "${collection_files[@]}"
cmake --preset "$collection_preset"
cmake --build --preset "$collection_preset" --parallel 4
ctest --preset "$collection_preset"
if [[ "$collection_preset" != asan ]]; then
  if command -v clang-tidy >/dev/null 2>&1; then
    collection_sources=()
    while IFS= read -r -d '' collection_source; do
      collection_sources+=("$collection_source")
    done < <(find src -name '*.cpp' -print0)
    clang-tidy -p "build/$collection_preset" "${collection_sources[@]}" --quiet
  elif [[ "$(uname -s)" == Darwin ]]; then
    python3 scripts/analyze_macos.py "build/$collection_preset/compile_commands.json"
  else
    echo "clang-tidy is required for static analysis" >&2
    exit 1
  fi
fi
git diff --check
