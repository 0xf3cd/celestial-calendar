---
name: using-celestial-calendar
description: >-
  Help an application or script use CelestialCalendar through its public Python,
  JavaScript/TypeScript, C++ or C API. Use for Gregorian/Chinese lunar conversion,
  Jieqi (节气), Sun/Moon positions, charts, houses and supported ephemeris-event
  queries with this library, including requests such as “公历转农历”, “下一个节气”,
  “UTC 转北京时间的节气” and “用 CelestialCalendar 算星盘”. Check the installed
  interface, time scales, units, domains and errors before producing runnable code.
  Not for contributing to the library, calendar UI design, or Ganzhi/Bazi interpretation.
---

<!-- Copyright (C) 2026 Ningqi Wang (0xf3cd). SPDX-License-Identifier: MIT -->

# Use CelestialCalendar

## Establish the environment

Identify the language, installed package or source checkout, and the operation the
user needs. Prefer the task's supplied version-matched documentation. The links
below describe current source, which can include unreleased interfaces; a package
version string alone does not prove that an operation exists.

Read the relevant language guide before writing a call:

- [Python](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/python/README.md)
  · [中文](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/python/README_ZH.md)
- [JavaScript / TypeScript](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/javascript/README.md)
  · [中文](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/javascript/README_ZH.md)
- [C++ quickstart](https://github.com/0xf3cd/celestial-calendar/blob/main/README.md)
- [C and public API guide](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/API.md)

Inspect the actual public module when interface availability matters. In Python,
check that the required attributes are callable. In JavaScript, use a namespace
or dynamic import so a missing named export can be handled, check the namespace
and initialization function, then await initialization before calculations.
Do not infer availability from an unexported package metadata subpath.

If the installed interface lacks the operation, explain that limitation and offer
the guide's compatible-package or source-build route. Ask before changing the
user's environment. Do not invent a method, substitute a different time scale,
or implement a replacement astronomical calculation to hide a missing interface.

## Read the task contract

| Task | Read next |
|---|---|
| Gregorian / lunar dates and algorithm selection | The language guide's lunar examples and [models and domains](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/ALGORITHMS.md) |
| UTC input, next Jieqi, local display | [UTC conversions and runnable Jieqi programs](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/UTC_CONVERSIONS.md) |
| Sun/Moon positions, complete chart or independent houses | The language guide's corresponding example, then the [API guide](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/API.md) |
| Longitude crossings, stations or two-body events | [Event contracts and interface availability](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/EPHEMERIS_EVENTS.md) |
| Time-scale conventions and failure categories | [Design](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/DESIGN.md) |

Use the public capability query only for the operations that its language guide
documents. It is not a universal method-discovery API. JavaScript does not expose
the interval-event API; explain that boundary and offer a supported language
instead of borrowing method names from Python or C++.

Before choosing inputs or interpreting results, check the relevant contract:

- Distinguish UTC, UT1 and TT. A timezone offset changes display, not time scale.
  A Jieqi's UT1 moment is not directly a UTC or UTC+8 clock reading. Follow the
  linked UTC example for conversion and selection across a year boundary.
- For lunar conversion, establish the civil date and algorithm's date basis.
  Do not let the host timezone silently choose the date.
- Keep each operation's angle, distance and rate units. In particular, do not
  assume standalone Moon distance and chart distance share a unit.
- Check input domains, model selection and documented historical/leap-second
  limits. Read those policies from the shared guide rather than extrapolating
  an example or assuming an exact roundtrip.
- Distinguish a valid empty result from an invalid input or failed calculation.
  Use the binding's documented error handling; do not turn errors into zeros or
  partial success.

## Deliver a program the user can run

Adapt the smallest supported public example. Include imports, initialization,
explicit input scale and units, and the command to run it. Execute it in the
user's selected environment when tools permit. Report the actual output or state
that execution was not verified; keep any original failure visible when fixing it.

Explain which fields answer the question and identify the documentation and
package/source used. Keep interpretation outside the calculation API: this
library's astronomy and calendar results do not supply Ganzhi/Bazi judgments.
