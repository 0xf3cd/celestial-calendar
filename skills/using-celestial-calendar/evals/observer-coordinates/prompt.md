---
tags: [consumer, python, observer, discovery]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

I use celestial-calendar in Python for a local sky display. At
2026-11-07T00:00:00Z, what are the Moon's altitude, azimuth and distance from a
WGS84 site at latitude -33.85°, east longitude 151.2°, elevation 50 m?
Show the geometric altitude and, separately, the altitude with Bennett
refraction at 15°C and 1013.25 hPa. Explain the azimuth convention.

Find the public interface in the supplied guides. Write and execute solution.py,
accepting the UTC ISO timestamp as its argument. Print JSON with angles in
degrees, distance in AU, and null for any absent angle. Do not install packages,
estimate refraction yourself or substitute geocentric coordinates.

docs/index.json maps public URLs to frozen local guides; use those rather than
network access. runtime.json identifies the installed environment and how to run
Python. Do not read evaluation graders or answer keys.
