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
The evaluator's shell sandbox can deny reads under the user's home directory, so
base a Python environment on an interpreter installed elsewhere and check that it
starts under an equivalent read restriction.

Create consumer working directories outside the contributor checkout. Supply the
same frozen public guides and runnable examples to both arms. Each consumer
directory gets a `docs/index.json` that maps each public URL to a local path and
records its source commit and SHA-256, and a `runtime.json` that records the
provided interpreter/package environment. Do not rewrite the skill to point to
those copies. Missing mapped material invalidates the
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
result. With the evaluated artifact, UT1 and UTC differ by about 2 ms at the Jieqi
selected by the utc-jieqi prompt, so a program that shows the UT1 moment as UTC
passes a millisecond comparison there; use held-out cutoffs where the difference
is larger. An artifact-derived expectation establishes consistency with that
artifact, not independent astronomical accuracy. Existing numerical goldens and
tolerances are unchanged.

## Results

On 2026-11-02, Claude Code 2.1.316 ran every case with Claude Sonnet 5.5 as agent
and judge, one run per arm. The positive cases used a development wheel built by
this repository's CI (SHA-256
`e8c6e69a90444706dddcaf251d6446e71b7b5c728003da19ba4f7f5093178ab8`) and a locally
built development npm tarball (SHA-256
`b3714c9545cba5422ad2f17602212242c51f242288693a0e4bbd2503c91e3fb6`); both come
from runtime sources identical to the frozen documentation's source commit,
`67a7c4c`. missing-interface ran once against the registry 1.0.0 Python wheel and
once against the registry 1.0.0 npm package, so there were six pairs.

Each of the twelve sessions started in a fresh directory with no user instructions
and an empty auto-memory, and its shell could not read the grader files. No trace
showed a grader read, network access or package installation. In gregorian-lunar,
utc-jieqi and chart-snapshot, each arm's first program execution succeeded, and
the unchanged program matched the installed artifact's public results, including
on held-out inputs for the two cases that take one. In the three limitation runs
(missing-interface against each old package, and unsupported-js-events), both arms
reported the missing operation without substituting another. Agent time totalled
119 s with the skill and 122 s without it. All six pairs tie: on these tasks,
Claude Sonnet 5.5 did as well without the skill. One run per arm is a small
sample.

The trigger queries ran separately, once each with the skill installed. All ten
positive queries loaded the skill and none of the ten near misses did. The queries
were written with the skill, and the session had no competing library skill, so
this checks the description's wording; triggering among many installed skills was
not measured.

The LLM grader gave every arm full marks. It also passed both arms of an earlier
diagnostic in which the evaluator's sandbox rejected every shell command before
the program ran, so its score is not reported as a result.
