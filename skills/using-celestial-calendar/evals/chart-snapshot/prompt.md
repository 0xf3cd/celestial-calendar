---
tags: [consumer, python, chart]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

Write and run solution.py using the installed celestial_calendar public API. Query
a chart for 2026-01-01 12:00 UTC at latitude 51.5 degrees north, longitude 0 degrees,
using Placidus and the default model. Print JSON containing the resolved UT1 and
TT times, Moon distance and its unit, node distances, all body longitude rates
and their unit, and the twelve house cusps in house order. Explain any absent
distance values. Do not install or upgrade packages.

docs/index.json maps public documentation URLs to frozen local copies; use those
instead of network access. runtime.json identifies the supplied environment.
Do not read evaluation graders or answer keys.
