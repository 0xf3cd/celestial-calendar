# Copilot Instructions — celestial-calendar

C++23 astronomical-calculation library (Gregorian ↔ Lunar ↔ Ganzhi, Jieqi, sun/moon positions, sunrise/sunset). Conventions live in `AGENTS.md` (single source of truth) — read it first.

## Review focus

- **Numerical correctness is the product**: every algorithm traces to a named reference (Meeus, VSOP87D, ELP2000-82B, SOFA, USNO ΔT). Flag any formula term, sign, or constant that deviates from its cited reference; never "simplify" formulas or drop terms.
- **Constants discipline**: cross-formula physical constants must be named `constexpr` (UPPER_CASE). Single-use coefficients of a cited reference polynomial stay inline in the formula with the `@ref` alongside; dense coefficient tables (e.g. `MEEUS_NUTATION_COEFFS`) keep raw literals with a source comment. `.clang-tidy` disables magic-number checks for exactly these two cases.
- **Tolerances are sacred**: tests use golden datasets + `ASSERT_NEAR`. Any PR that loosens an existing tolerance is a blocker unless it documents the algorithm/reference justification. New numerics require a new reference dataset + tolerance.
- **Header-only by design**: logic in `src/**/*.hpp` with `#pragma once`, functions `inline`, shared constants `inline constexpr`. `.cpp` only in `shared_lib/` (C-ABI). No `using namespace` or namespace-scope `using` in headers.
- **Toolchain**: C++23, clang++ (LLVM 18) / g++ 14; `clang-tidy` runs `WarningsAsErrors: '*'` — zero warnings tolerated. Respect the deliberate `.clang-tidy` disables instead of "fixing" code to satisfy them.
- Naming: `lower_case` variables/functions, `CamelCase` types, `UPPER_CASE` constants/enums.
