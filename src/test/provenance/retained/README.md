# Retained sources and values

`record.json` lists source dispositions and their evidence. `retained_host_blocks.json`
records the files or blocks containing retained material (hosts), their sources, notices,
licence boundaries, and value digests. Upstream snapshots and grants have separate identity checks.

## Register material

Give each host an in-file boundary or an adjacent attribution section, identify its source
and owner, and register any applicable notice. A project source cannot become exempt from
the project SPDX rule merely by joining the exception list.

For a C++ data block, add a unique `// Retained data: <id>.` start comment and register its
path, start, end, method, and value digest. Printed reference inputs use
`// Retained reference inputs/values: <id>.`. Each start requires exactly one entry;
end comments are not starts. A non-marker entry needs an actual host or record-evidence
reference. Keep the source citation and tolerance rationale beside the data.

Notebook entries identify saved outputs and explicit year/Jieqi input pairs, not execution
counters or presentation metadata. Clearing outputs does not retire these input pairs.
The binding fixture's producer path is owned by `automation.paths`; its replays
must still read that same file. Adjacent whole-file hosts independently check their
`data_sha256`, including the Moon CSV and USNO data file.

## Retire material

Remove the actual retained block or input first, then update its host, value entry,
record evidence, attribution section, notices, and live producer or replay callers as
applicable. Removing a start marker in a surviving host retires a value guard and needs
the same review as deleting its entry. Do not rewrite frozen upstream records or
historical changelog prose.

The structural checks detect surviving markers, evidence, and orphan markings. They do
not authenticate a coordinated deletion of every registration and source anchor; review
the complete retirement diff against the prior source.

Run `python -m automation.provenance` to check the source identities and committed notice
bundle directly. Retained-value mutations and ordinary-maintenance controls live in
`automation/tests/test_retained_provenance.py`, `test_retained_maintenance.py`, and
`test_provenance.py`.
