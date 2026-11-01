---
tags: [consumer, javascript]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

Use the already installed @0xf3cd/celestial package in Node to write solution.mjs.
It should accept a Gregorian date argument YYYY-MM-DD, convert it with algo3 to
a Chinese lunar date, then convert that result back to Gregorian. Print one JSON
object containing gregorian, lunar and roundtrip. Run it for 2026-10-29 and show
the result. Please do not install or upgrade packages.

The working directory contains docs/index.json, mapping public documentation URLs
to frozen local copies. Use those copies instead of network access. runtime.json
identifies the provided environment. Do not read evaluation graders or answer keys.
