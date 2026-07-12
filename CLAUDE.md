# CLAUDE.md — celestial-calendar

C++23 astronomical-calculation library: Gregorian ↔ Lunar ↔ Ganzhi conversions,
Jieqi (节气) moments, sun/moon positions, sunrise/sunset. 天文计算与历法转换.

This file tells any AI assistant how to write code **in the author's style**, so
contributions stay indistinguishable from hand-written code. For *style* questions,
read a neighbouring header and imitate it rather than inventing a convention.

## Human-in-the-loop (read first)
Stop and confirm with the author before proceeding whenever anything is unclear or
non-obvious — an ambiguous requirement, a design fork with no precedent in the code, an
algorithm/reference you're unsure of, an unexpected test failure, or any change whose
intent you can't verify from existing code. Prefer asking over guessing.

## Correctness is numerical, measured against references
This is precision astronomy, not vibes. Every algorithm traces to a named reference
(Meeus, VSOP87D, ELP2000-82B, SOFA, USNO ΔT — see README §References).
- **Name meaningful constants** as `constexpr` (UPPER_CASE), like the existing code.
  Raw numeric literals belong ONLY inside cited reference coefficient tables
  (e.g. `MEEUS_NUTATION_COEFFS`) — that dense data can't be named element-by-element,
  which is the only reason `.clang-tidy` disables the magic-number checks. It is NOT a
  licence for bare literals in formulas. Every table carries a source comment.
- Never "simplify" a formula or drop a term for cleaner code — the constants ARE the algorithm.
- Correctness is proven by **golden datasets**: tests hold high-precision reference values
  and `ASSERT_NEAR` to a per-column tolerance. New numerics ⇒ add a reference dataset + tolerance.
- **Never loosen an existing tolerance on your own.** Before touching a tolerance: talk to the
  author, debug it carefully, and confirm case-by-case that it's an algorithm/reference issue,
  not a code bug. The default is to surface the discrepancy, not to make the test green.
- Cite the source of any new algorithm / coefficients in a comment.

## Toolchain
Build / lint / test commands live in **README.md §3–4** (`./project.py --all`,
`./linter.py --ruff --clang-tidy`) — follow those, don't restate them here.
- C++23; clang++ (LLVM 18) or g++ 14. All build/lint/CI is Python-orchestrated
  (`project.py`, `linter.py`, `toolbox/`).
- `clang-tidy` runs with **`WarningsAsErrors: '*'`** — zero warnings. `.clang-tidy` is the
  config; respect its deliberate disables (magic-numbers, identifier-length, identifier-naming)
  instead of "fixing" code to satisfy a disabled check.

## Naming (SSOT: `.clang-tidy` CheckOptions)
`lower_case` — variables / functions / params / members / methods / namespaces.
`CamelCase` — class / struct.  `UPPER_CASE` — global constants / enums / enum constants.

## Header-only, and the code reads like the maths
Header-only is a **deliberate design choice** — the no-link, orthogonal, self-contained feel
is intentional. Keep it. That buys a discipline:
- All logic in `src/**/*.hpp` with `#pragma once`; every function `inline`, shared constants
  `inline constexpr`. `.cpp` exists only in `shared_lib/` for the C-ABI exports. New logic → a header.
- **No `using namespace` and no namespace-scope `using` declarations in headers** — they leak
  into every including TU and cause conflicts / ambiguity. Put `using` inside function bodies
  (as `sun.hpp` already does with `using namespace astro::toolbox::literals;`) or fully-qualify.
  Existing violations tracked in #51.
- **Nested `lower_case` namespaces** by domain (`astro::earth::nutation`); close with a
  `} // namespace …` comment.
- **Trailing return types**: `inline auto f(const double jde) -> SphericalCoordinate`.
- **Unicode math identifiers are deliberate** (`λ`, `β`, `Δψ`, `Ω`, `Mp`). Name symbols as the
  papers do; do NOT ASCII-ise them.
- **Strong physical types** over bare doubles (`Angle<RAD>`, `Distance<AU>`, `SphericalCoordinate`)
  with designated initializers (`.λ = …`). Keep units in the type system.
- `const`-correct; modern C++ (`<ranges>`, `<span>`, `std::array`).
- Doxygen `/** @brief / @param / @return */` on public functions; inline `//` comments explain
  the physics, not the syntax.

## File header
Every file opens with the full GPL-3.0 block comment (copy from any src file), verbatim except
the year: `Copyright (C) <year> Ningqi Wang (0xf3cd)`. New files use the current year; never
change an existing file's year. GPL-3.0 project — keep the header.

## Tests
- GoogleTest `TEST(Suite, Case)` in `src/test/**`, mirroring `src/**`
  (`astro/earth.hpp` → `test/astro/earth_test.cpp`), namespace `…::test`.
- Data-driven: inline a column-aligned dataset of reference values; `ASSERT_NEAR` with a
  per-column tolerance; header-comment the columns.

## AI do / don't
- DON'T round / drop astronomical constants or loosen tolerances to pass CI (see above).
- DON'T ASCII-ise unicode identifiers, move logic out of headers, or add namespace-scope
  `using` to a header.
- DON'T add a dependency or build step outside the `project.py` / `linter.py` flow.
- Match the neighbouring header's texture; internal consistency > external "best practice".
