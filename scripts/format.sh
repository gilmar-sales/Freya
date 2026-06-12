#!/usr/bin/env bash
set -euo pipefail

MODE="write"
if [[ "${1:-}" == "--check" ]]; then
  MODE="check"
fi

mapfile -t FILES < <(
  git ls-files \
    | grep -E '\.(c|cc|cpp|cxx|h|hpp|hh|inc)$' \
    | grep -v -E '(^|/)Shaders/' \
    | grep -v -E '(^|/)src/Freya/Vendor/'
)

if [[ "${#FILES[@]}" -eq 0 ]]; then
  echo "No source files to format."
  exit 0
fi

case "$MODE" in
  write)
    clang-format -i --Werror "${FILES[@]}"
    echo "Formatted ${#FILES[@]} files."
    ;;
  check)
    if ! clang-format --Werror -n "${FILES[@]}"; then
      echo
      echo "ERROR: above files are not clang-format clean. Run './scripts/format.sh'." >&2
      exit 1
    fi
    echo "All ${#FILES[@]} files are clang-format clean."
    ;;
esac
