---
tags: [consumer, javascript, negative-control]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

I use @0xf3cd/celestial in Node. Can its public JavaScript API find every Mercury
longitude station in a caller-chosen TT interval? Check the installed package
and matching public documentation. If supported, show a runnable call. Otherwise
explain what interface I would need. Please don't invent a wrapper, implement a
root finder or install anything.

docs/index.json maps public documentation URLs to frozen local copies. Use those
instead of network access. runtime.json identifies the supplied environment.
Do not read evaluation graders or answer keys.
