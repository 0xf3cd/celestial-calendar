---
tags: [consumer, compatibility, negative-control]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

runtime.json identifies an installed older CelestialCalendar interface and the
language to use. Check whether it can convert a UTC civil time to JDE(TT) and
back through maintained public functions. If it can, demonstrate that with
2026-10-28T19:35:37Z. If it cannot, explain the limitation and the documented
compatible-package or source-build route. Do not upgrade, install, implement
replacement conversions, or use a chart query as a conversion bridge.

docs/index.json maps public documentation URLs to frozen local copies. Use those
instead of network access. Do not read evaluation graders or answer keys.
