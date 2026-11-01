<!-- Copyright (C) 2026 Ningqi Wang (0xf3cd). SPDX-License-Identifier: MIT -->

# CelestialCalendar consumer skill

This skill helps an assistant call the library in an application or script. It
routes tasks to the existing public guides and checks interface availability,
time scales, units, domains and errors. Contributor instructions remain in
[AGENTS.md](../../AGENTS.md).

## Install

Copy [SKILL.md](SKILL.md) into a directory named `using-celestial-calendar` in
your assistant's supported skill location, then enable it using that assistant's
instructions. SKILL.md is the complete portable unit: its references are public
URLs, so copying it alone works. You can also copy this whole directory; the
evaluation material is for maintainers and is not needed for normal use.

Installing the skill does not install the calculation library. Use the
[Python](../../bindings/python/README.md),
[JavaScript](../../bindings/javascript/README.md) or [C++ guide](../../README.md)
for that. Current-source guides can describe unreleased interfaces; use the
documentation matching the package you actually install.

## Try it

- “Use CelestialCalendar in Node to convert 2026-10-29 to a Chinese lunar date.”
- “给定 UTC 时刻，用 Python 找下一个节气，显示北京时间。”
- “Query a chart at an explicit UTC instant; show the resolved times and Moon distance.”

The body is English and includes Chinese trigger examples and links to Chinese
guides. It does not add calculation APIs or a separate API reference.

## Evaluate

The cases in [evals](evals) compare fresh sessions with and without the skill.
[The protocol](EVALUATION.md) specifies artifact identity, frozen public inputs,
first-execution evidence and failure controls. A passing local structure check
does not establish that generated programs work or that the skill improves them.
