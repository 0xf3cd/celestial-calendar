# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Execute displayed Python programs against the active interpreter's installed wheel."""

from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
# These fences are checked by other consumers or are instructions/output, not Python programs.
NON_PROGRAMS = {
  "sh": "shell installation/build instructions",
  "powershell": "PowerShell build instructions",
  "c": "C quickstart, compiled separately",
  "cpp": "C++ quickstart, compiled by source automation",
  "js": "JavaScript program, executed by the npm consumer",
  "text": "displayed expected output",
}


def read_fences(text: str) -> list[tuple[str, str]]:
  """Read the guides' top-level fences; source tests compare every block with CommonMark."""
  blocks = []
  opening = None
  body = []
  comment = False
  for line in text.replace("\r\n", "\n").splitlines(keepends=True):
    if opening is not None:
      marker, info, indent = opening
      if re.fullmatch(rf" {{0,3}}{re.escape(marker[0])}{{{len(marker)},}}[ \t]*\n?", line):
        blocks.append((info, "".join(body)))
        opening, body = None, []
      else:
        body.append(line[min(indent, len(line) - len(line.lstrip(" "))) :])
      continue
    if comment:
      comment = "-->" not in line
      continue
    if line.lstrip().startswith("<!--"):
      comment = "-->" not in line
      continue
    match = re.fullmatch(r"( {0,3})(`{3,}|~{3,})([^\n]*)\n?", line)
    if match:
      indent, marker, info = match.groups()
      if marker[0] == "`" and "`" in info:
        raise AssertionError("unsupported fence info")
      opening = marker, info.strip(), len(indent)
  assert opening is None, "unclosed documentation fence"
  return blocks


def programs(text: str) -> list[str]:
  """Classify every fence and require at least one executable Python program."""
  blocks = read_fences(text)
  unknown = {info for info, _ in blocks} - {"python", *NON_PROGRAMS}
  assert not unknown, f"unclassified documentation fences: {sorted(unknown)!r}"
  selected = [body for info, body in blocks if info == "python"]
  assert selected and all(body.strip() for body in selected), "missing Python documentation programs"
  return selected


def expected_output(text: str) -> str:
  """Use the root guide's single displayed output, without another expected-value copy."""
  outputs = [body for info, body in read_fences(text) if info == "text"]
  assert len(outputs) == 1 and outputs[0].strip(), "expected exactly one displayed output"
  return outputs[0]


def run_program(code: str, cwd: Path, expected: str | None = None) -> str:
  """Run an unmodified standalone program without inherited imports or checkout paths."""
  assert not cwd.resolve().is_relative_to(REPO), "examples must run outside the checkout"
  source = cwd / "example.py"
  source.write_text(code, encoding="utf-8")
  result = subprocess.run(
    [sys.executable, "-I", "-X", "utf8", str(source)], cwd=cwd, capture_output=True, encoding="utf-8"
  )
  assert result.returncode == 0, (
    f"example exited {result.returncode}: stdout={ascii(result.stdout)} stderr={ascii(result.stderr)}"
  )
  stdout = result.stdout.replace("\r\n", "\n")
  if expected is not None:
    assert stdout == expected, f"displayed output mismatch: {ascii(stdout)}"
  return stdout


def metadata_description(raw: str) -> str:
  """Read raw UTF-8 METADATA text, not an email parser's decoded payload."""
  _, separator, description = raw.replace("\r\n", "\n").partition("\n\n")
  assert separator and description, "missing METADATA description"
  return description


def verify_provenance(record: dict) -> None:
  """Require both Python and native modules to belong to the active installed distribution."""
  files = {Path(file).resolve() for file in record["files"]}
  prefix = Path(record["prefix"]).resolve()
  for key in ("package", "native"):
    path = Path(record[key]).resolve()
    assert path in files and path.is_relative_to(prefix), f"not an installed distribution member: {key}"
    assert not path.is_relative_to(REPO / "bindings/python/src"), f"checkout import: {key}"


def main() -> None:
  with tempfile.TemporaryDirectory(prefix="celestial-doc-examples-") as temporary:
    cwd = Path(temporary)
    provenance = run_program(
      "import importlib.metadata, json, sys\n"
      "import celestial_calendar\n"
      "from celestial_calendar import _binding\n"
      "dist = importlib.metadata.distribution('celestial-calendar')\n"
      "print(json.dumps(dict(package=celestial_calendar.__file__, native=str(_binding.LIB._name),\n"
      "  prefix=sys.prefix, files=[str(dist.locate_file(p)) for p in dist.files],\n"
      "  metadata=dist.read_text('METADATA'))))\n",
      cwd,
    )
    record = json.loads(provenance)
    verify_provenance(record)
    installed = metadata_description(record["metadata"])
    guide = (REPO / "bindings/python/README.md").read_text(encoding="utf-8")
    assert installed == guide, "installed METADATA description differs from candidate guide"
    print("PASS installed distribution provenance and raw METADATA guide bytes")

    for name, text, output in (
      ("root", (REPO / "README.md").read_text(encoding="utf-8"), True),
      ("python-package", installed, False),
    ):
      selected = programs(text)
      executed = 0
      for code in selected:
        run_program(code, cwd, expected_output(text) if output else None)
        executed += 1
      assert executed == len(selected)
      print(f"PASS displayed {name} Python programs {executed}/{len(selected)}")


if __name__ == "__main__":
  main()
