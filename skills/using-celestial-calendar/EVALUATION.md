<!-- Copyright (C) 2026 Ningqi Wang (0xf3cd). SPDX-License-Identifier: MIT -->

# Consumer evaluation protocol

Compare the same task, model, installed artifact and public documentation in fresh
with-skill and without-skill sessions. Only skill availability changes. Save each
prompt, skill digest, tool trace, generated program, direct execution result and
human disposition. Report ties and regressions as well as improvements.

## Prepare the inputs

Use exact Python wheel and npm tarball files, not a latest-version selection or
filename glob. Record each digest, version and source/producer provenance. Probe
the installed public interface; a version label alone cannot establish availability.
Keep old-interface controls separate from current-source positive cases.

Create consumer working directories outside the contributor checkout. Supply the
same frozen public guides and runnable examples to both arms. `docs/index.json`
maps each public URL to a local path and records its source commit and SHA-256.
Do not rewrite the skill to point to those copies. `runtime.json` records the
provided interpreter/package environment. Missing mapped material invalidates the
case; do not fetch moving development documentation during the run.

Keep graders outside both the consumer cwd and installed skill directory. Inspect
actual user-level instructions, auto-memory, visible skills and tool traces in
both arms. Private contributor context, a grader read, or insufficient evidence
to exclude them makes the comparison invalid or unknown. A fresh cwd alone does
not prove isolation. Stop rather than moving credentials to another profile.

## Run and inspect

Start with one paired case to check the native runner's actual child cwd, installed
environment access and complete trace capture. Use the same tool permissions in
both arms. Permit local reads and program execution, not package installation,
network access, nested agents or runtime replacement. Keep reports local; for
Claude Code's native evaluator explicitly use `--no-publish` and verify that the
requested debug file contains the verbose trace.

Pin both agent and judge models and begin with one run per arm. Record actual
concurrency, timing and available token metrics; missing metrics stay unknown.
Distinguish below-threshold scoring from a failed or partial run using its result
records, not its process exit code alone. Set explicit cost and concurrency bounds.

Reconstruct the first program execution from complete write/edit/shell inputs and
the corresponding tool results. Save those bytes, arguments, environment, exit
and output separately from later self-repair and parent replay. If reconstruction
is impossible, mark first execution unknown. Never grade a hand-repaired program
as the model's original success.

Deterministic replay/result checks decide execution correctness. The case's LLM
grader adds a separate trace judgment; its score alone is not an execution oracle.
Use a second input to reject hardcoded answers. For a UTC/Jieqi case, require
fractional-second output and prove wrong-scale controls differ in the checked
result. An artifact-derived expectation establishes consistency with that
artifact, not independent astronomical accuracy. Existing numerical goldens and
tolerances are unchanged.

## Results

The 2026-11-01 diagnostic used Claude Code 2.1.316 with Claude Sonnet 5.5 for
both agent and judge, one run per arm, and the Gregorian/lunar task. Both arms
read public material and wrote programs, but the evaluator environment rejected
their shell execution before Node ran. The LLM grader nevertheless marked both
as passing. Those flags were rejected: this is an invalid execution comparison,
not evidence of a tie or improvement. The other cases have not been run.

Structure checks, a plan review or a model's written answer must not be reported
as a measured skill improvement. A valid comparison still requires executable
consumer sessions, complete context evidence and deterministic result checks.
