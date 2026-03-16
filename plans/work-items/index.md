# Work Items

This index tracks the current backlog extracted from [[../reviews/review-concensus]].

Priority order:

1. [[01 app loop and ui request decomposition -feature]]
2. [[02 render test extraction and parser unification -feature]]
3. [[03 public api boundary cleanup -feature]]
4. [[05 vulkan robustness and atlas upload rework -bug]]
5. [[06 test seam cleanup and lifecycle coverage -test]]
6. [[07 platform text and display integration -feature]]
7. [[08 gui ergonomics and observability -feature]]

Completed (moved to `../work-items-complete/`):

- 00 rpc transport serialization and shutdown hardening
- 04 grid wide-cell repair and redraw robustness

Execution rule:

- finish the correctness items first
- keep each item independently buildable and testable
- prefer small, reviewable slices over another large cross-module refactor
- after completing each work item, run the full unit tests and the smoke test (`ctest --test-dir build`) to verify no regressions
- after completing work on a slice or item, update the work item file to mark completed steps as done
