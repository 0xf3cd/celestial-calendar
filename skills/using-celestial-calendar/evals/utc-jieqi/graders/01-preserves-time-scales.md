---
type: llm
focus: trace
weight: 1
---

The executed program converts the UTC cutoff and each candidate UT1 moment to TT
before selection, then uses the public inverse UTC conversion and a separate fixed
UTC+8 display offset, without a chart query or replacement time-scale arithmetic.
