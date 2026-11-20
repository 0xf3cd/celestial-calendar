---
tags: [consumer, javascript, positions, longitude-events, discovery]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

I use @0xf3cd/celestial in Node. Query the Moon and true ascending lunar node at
JDE(TT) 2451545.0 without calculating a full chart. Also find Mercury's longitude
stations in the half-open TT interval [2460735.5,2460785.5).

Check the installed interface and supplied guides. Write solution.mjs accepting
the position JDE(TT), interval start and interval end as three arguments, and
execute it. Print JSON with both positions in caller order, distances in AU
(null when the node has no distance), and each station's TT time, native
longitude, search rate and motion before/after. Do not normalize the returned
event longitude or force its reported search rate to zero.

docs/index.json maps public URLs to frozen local guides; use those rather than
network access. runtime.json identifies the installed environment and Node
command. Do not invent wrappers, install anything, implement a numerical search
or read evaluation graders or answer keys.
