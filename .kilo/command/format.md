---
description: Run clang-format on every project source file (or check with --check)
---

Run `bash scripts/format.sh $ARGUMENTS` from the repo root.

- No arguments: format files in place.
- `--check`: verify formatting and exit non-zero on drift.

The script uses `.clang-format` at the repo root and skips `build/`, `Shaders/`,
`src/Freya/Vendor/`, and `.cache/`. It only operates on git-tracked files.
