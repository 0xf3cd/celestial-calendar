---
tags: [consumer, javascript, windows, discovery]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

Using @0xf3cd/celestial in Node, find the observing windows in which Mars's
geometric centre is above 20° and the Sun's geometric centre is below -18°.
The WGS84 site is latitude -33.85°, east longitude 151.2°, elevation 50 m.
Use the half-open TT interval [2460676.5,2460677.5).

Discover the public API, write solution.mjs accepting start and end JDE(TT)
arguments, and execute it. Print a JSON list of the returned windows with both
TT endpoints and their converted UTC calendar values. Keep an empty result
distinct from failure. Explain what the search can miss; do not implement a
root finder or claim that successful output enumerates every possible window.

docs/index.json maps public URLs to frozen local guides; use those rather than
network access. runtime.json identifies the installed environment and Node
command. Do not install anything or read evaluation graders or answer keys.
