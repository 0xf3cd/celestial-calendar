---
tags: [consumer, python, rise-set, discovery]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

I need daily Sun, Moon and Mars rise, upper transit and set for Tromsø:
latitude 69.65°, east longitude 18.96°, WGS84 elevation 0 m.
Use Gregorian UT1 date 2025-06-21, not local midnight, and each query's standard
altitude/weather defaults.

Find the supported Python calls and write solution.py accepting a YYYY-MM-DD
argument. Execute it for that date and print JSON: each body's TT event numbers,
converted UTC calendar values, null for missing events, and the polar state
separately. Explain the different solar versus lunar/planetary date selection.
Do not assume a missing sunrise is an error or relabel a TT number as UTC.

docs/index.json maps public URLs to frozen local guides; use those rather than
network access. runtime.json identifies the installed environment and how to run
Python. Do not install anything or read evaluation graders or answer keys.
