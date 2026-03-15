# Work Items

This index tracks the current backlog extracted from [[../reviews/review-concensus]].

Priority order:

1. [[00 rpc transport serialization and shutdown hardening -bug]]
2. [[01 app loop and ui request decomposition -feature]]
3. [[02 render test extraction and parser unification -feature]]
4. [[03 public api boundary cleanup -feature]]
5. [[04 grid wide-cell repair and redraw robustness -bug]]
6. [[05 vulkan robustness and atlas upload rework -bug]]
7. [[06 test seam cleanup and lifecycle coverage -test]]
8. [[07 platform text and display integration -feature]]
9. [[08 gui ergonomics and observability -feature]]

Execution rule:

- finish the correctness items first
- keep each item independently buildable and testable
- prefer small, reviewable slices over another large cross-module refactor
