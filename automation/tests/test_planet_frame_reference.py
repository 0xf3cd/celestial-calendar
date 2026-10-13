# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import hashlib
import importlib.util
import json
import re
import subprocess
import sys

from pathlib import Path

import pytest

from automation.source_digest import canonical_cpp


REPO_ROOT = Path(__file__).resolve().parents[2]
GENERATOR = REPO_ROOT / "statistics/planet_frame_reference.py"


@pytest.fixture(scope="module")
def reference():
  spec = importlib.util.spec_from_file_location("planet_frame_reference", GENERATOR)
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


def _array(name, amplitude):
  return f"constexpr std::array<VSOP87Coefficient, 1> {name} {{{{ {{ {amplitude}, 0.0, 0.0 }} }}}};\n"


def _aggregate(name, refs):
  terms = ", ".join(f"{{ {ref}.data(), {ref}.size() }}" for ref in refs)
  return f"constexpr std::array<VSOP87Coefficient2, {len(refs)}> {name} {{{{ {terms} }}}};\n"


@pytest.fixture
def synthetic(tmp_path, reference):
  # Constant, two-power tables exercise the parser and CLI; these are not physical oracles.
  root = tmp_path / "reference"
  root.mkdir()
  (root / "AA+.htm").write_text("<title>AA+ v2.55</title>", encoding="ascii")
  checks = []
  for body, (name, code) in enumerate(zip(reference.NAMES, reference.CODES, strict=True)):
    lon, lat, radius = 1 + body / 8, -0.1, body + 1
    text = ""
    for axis, value in zip("LBR", (lon, lat, radius), strict=True):
      refs = [f"g_VSOP87D_{axis}{power}_{name}" for power in range(2)]
      text += _array(refs[0], f"{value:.12f}") + _array(refs[1], "0.0")
      text += _aggregate(f"g_VSOP87D_{axis}_{name}", refs)
    (root / f"AAVSOP87D_{code}.cpp").write_text(text, encoding="ascii")
    checks.extend((f"VSOP87D {name} JD2451545.0", f"l {lon:.12f} rad b {lat:.12f} rad r {radius:.12f} au"))
  check_file = tmp_path / "vsop87.chk"
  check_file.write_text("\n".join(checks) + "\n", encoding="ascii")
  return root, check_file


def test_parse_complete_synthetic_tables(reference, synthetic):
  root, _ = synthetic
  tables, source = reference.parse_tables(root, 0)
  assert tables == {
    (axis, power): ((value if power == 0 else 0.0, 0.0, 0.0),)
    for axis, value in zip("LBR", (1.0, -0.1, 1.0), strict=True)
    for power in range(2)
  }
  assert source["counts"] == {axis: [1, 1] for axis in "LBR"}
  assert source["sha256"] == hashlib.sha256((root / source["file"]).read_bytes()).hexdigest()


@pytest.mark.parametrize(
  ("mutation", "message"),
  [
    ("term-count", "Incomplete coefficient array"),
    ("term-syntax", "Incomplete coefficient array"),
    ("stray-text", "Incomplete coefficient array"),
    ("duplicate-array", "Duplicate array"),
    ("missing-array", "Missing coefficient arrays"),
    ("aggregate-count", "Incomplete aggregate"),
    ("aggregate-order", "Incomplete aggregate"),
    ("aggregate-reference", "Incomplete aggregate"),
    ("missing-coordinate", "Missing coordinates"),
  ],
)
def test_parser_rejects_incomplete_source(reference, synthetic, mutation, message):
  root, _ = synthetic
  tables, source = reference.parse_tables(root, 0)
  assert len(tables) == 6 and source["counts"] == {axis: [1, 1] for axis in "LBR"}
  path = root / "AAVSOP87D_EAR.cpp"
  text = path.read_text(encoding="ascii")
  lrefs = ["g_VSOP87D_L0_EARTH", "g_VSOP87D_L1_EARTH"]
  aggregate = _aggregate("g_VSOP87D_L_EARTH", lrefs)
  if mutation == "term-count":
    changed = text.replace("VSOP87Coefficient, 1>", "VSOP87Coefficient, 2>", 1)
  elif mutation == "term-syntax":
    changed = text.replace("1.000000000000, 0.0, 0.0", "1.000000000000, 0.0, invalid", 1)
  elif mutation == "stray-text":
    changed = text.replace("{ 1.000000000000, 0.0, 0.0 }", "{ 1.000000000000, 0.0, 0.0 }, STRAY_TEXT", 1)
  elif mutation == "duplicate-array":
    changed = text + _array(lrefs[0], "1.0")
  elif mutation == "missing-array":
    changed = text + "constexpr std::array<VSOP87Coefficient, 1> missing;\n"
  elif mutation == "aggregate-count":
    changed = text.replace("VSOP87Coefficient2, 2>", "VSOP87Coefficient2, 3>", 1)
  elif mutation == "aggregate-order":
    changed = text.replace(aggregate, _aggregate("g_VSOP87D_L_EARTH", lrefs[::-1]))
  elif mutation == "aggregate-reference":
    changed = text.replace(aggregate, _aggregate("g_VSOP87D_L_EARTH", [lrefs[0], "g_VSOP87D_B1_EARTH"]))
  else:
    changed = text.replace(aggregate, "")
  assert changed != text
  path.write_text(changed, encoding="ascii")
  with pytest.raises(ValueError, match=message):
    reference.parse_tables(root, 0)


@pytest.mark.parametrize("mutation", ["missing-anchor", "wrong-anchor"])
def test_primary_checks_reject_incomplete_or_wrong_anchors(reference, synthetic, mutation):
  root, check_file = synthetic
  tables = tuple(reference.parse_tables(root, body)[0] for body in range(8))
  anchors = reference.primary_checks(tables, check_file)
  assert len(anchors) == 8 and all(anchor["errors"] == (0.0, 0.0, 0.0) for anchor in anchors)
  text = check_file.read_text(encoding="ascii")
  if mutation == "missing-anchor":
    changed = "\n".join(text.splitlines()[:-2]) + "\n"
    message = "Incomplete primary D checks"
  else:
    changed = text.replace("l 1.000000000000", "l 1.000000000100", 1)
    message = "Primary printed D substitution failed: EARTH"
  assert changed != text
  check_file.write_text(changed, encoding="ascii")
  with pytest.raises(ValueError, match=message):
    reference.primary_checks(tables, check_file)


def test_generate_rejects_wrong_documentation_version(reference, synthetic):
  root, check_file = synthetic
  assert len(reference.generate(root, check_file)["rows"]) == 41
  (root / "AA+.htm").write_text("<title>AA+ v2.54</title>", encoding="ascii")
  with pytest.raises(ValueError, match="Reference root does not identify AA\\+ v2.55"):
    reference.generate(root, check_file)


def test_cli_emits_json_for_complete_synthetic_root(synthetic):
  root, check_file = synthetic
  result = subprocess.run(
    [sys.executable, str(GENERATOR), "--reference-root", str(root), "--check-file", str(check_file)],
    check=True,
    capture_output=True,
    text=True,
  )
  output = json.loads(result.stdout)
  assert result.stderr == ""
  assert len(output["rows"]) == 41 and len(output["primary_J2000_D_anchors"]) == 8
  assert len(output["sources"]) == 8
  source_files = [f"AAVSOP87D_{code}.cpp" for code in ("EAR", "MER", "VEN", "MAR", "JUP", "SAT", "URA", "NEP")]
  assert [source["file"] for source in output["sources"]] == source_files
  assert [source["sha256"] for source in output["sources"]] == [
    hashlib.sha256((root / name).read_bytes()).hexdigest() for name in source_files
  ]
  assert output["documentation_sha256"] == hashlib.sha256((root / "AA+.htm").read_bytes()).hexdigest()
  assert output["check_sha256"] == hashlib.sha256(check_file.read_bytes()).hexdigest()
  assert all(source["counts"] == {axis: [1, 1] for axis in "LBR"} for source in output["sources"])
  assert output["generator_sha256"] == hashlib.sha256(GENERATOR.read_bytes()).hexdigest()


@pytest.mark.parametrize("omit", ["both", "reference-root", "check-file"])
def test_cli_requires_explicit_inputs(synthetic, omit):
  root, check_file = synthetic
  args = []
  if omit == "reference-root":
    args = ["--check-file", str(check_file)]
  elif omit == "check-file":
    args = ["--reference-root", str(root)]
  result = subprocess.run([sys.executable, str(GENERATOR), *args], capture_output=True, text=True)
  assert result.returncode == 2 and result.stdout == ""
  assert "required" in result.stderr
  if omit != "check-file":
    assert "--reference-root" in result.stderr
  if omit != "reference-root":
    assert "--check-file" in result.stderr


def test_light_time_reports_nonconvergence_after_ten_iterations(reference):
  positive = reference.light_time(lambda _: (1.0, 0.0, 0.0), (0.0, 0.0, 0.0), reference.J2000)
  assert positive["iterations"] == 2 and positive["fixed_point_residual_days"] == 0.0
  calls = []

  def alternating_target(epoch):
    calls.append(epoch)
    return (1.0 if len(calls) % 2 else 2.0, 0.0, 0.0)

  with pytest.raises(RuntimeError, match="Independent light-time iteration failed"):
    reference.light_time(alternating_target, (0.0, 0.0, 0.0), reference.J2000)
  assert len(calls) == 10


def test_book_example_rejects_wrong_transport(reference, monkeypatch):
  lon, lat, radius = reference.book_example()
  assert lon == pytest.approx(118.704, abs=0.0005)
  assert lat == pytest.approx(1.615, abs=0.0005)
  assert radius == pytest.approx(1.0)
  monkeypatch.setattr(reference, "ecliptic_matrix", lambda initial, final: ((1, 0, 0), (0, 1, 0), (0, 0, 1)))
  with pytest.raises(ValueError, match="Meeus Example 21.c failed"):
    reference.book_example()


@pytest.mark.parametrize(
  "substitution",
  [(118.705, 1.615, 1.0), (118.704, 1.616, 1.0)],
  ids=["longitude-only", "latitude-only"],
)
def test_book_example_rejects_each_component(reference, monkeypatch, substitution):
  assert len(reference.book_example()) == 3
  monkeypatch.setattr(reference, "spherical", lambda vector: (118.704, 1.615, 1.0))
  assert reference.book_example() == (118.704, 1.615, 1.0)
  monkeypatch.setattr(reference, "spherical", lambda vector: substitution)
  with pytest.raises(ValueError, match="Meeus Example 21.c failed"):
    reference.book_example()


def test_current_source_record_matches_generator_rows_and_thresholds():
  pins = json.loads((REPO_ROOT / "src/test/provenance/planet-frame/SOURCE_PINS.json").read_text(encoding="utf-8"))
  assert pins["generator"] == GENERATOR.relative_to(REPO_ROOT).as_posix()
  assert pins["generator_sha256"] == hashlib.sha256(GENERATOR.read_bytes()).hexdigest()
  text = (REPO_ROOT / pins["host"]).read_text(encoding="utf-8")
  rows = re.search(r"constexpr std::array FRAME_ROWS \{.*?\n\};", text, re.S).group()
  assert pins["data"] == "FRAME_ROWS"
  assert pins["rows"] == len(re.findall(r"FrameRow \{", rows)) == 41
  assert pins["canonical_cpp_sha256"] == hashlib.sha256(canonical_cpp(rows).encode("utf-8")).hexdigest()
  tolerance = re.search(r"constexpr Tolerance FRAME_TOLERANCE \{(.*?)\};", text).group(1)
  values = [float(value) for value in re.findall(r"\.(?:lon_deg|lat_deg|r_au) = ([^,} ]+)", tolerance)]
  assert pins["tolerances_deg_deg_au"] == values == [1e-9, 2e-11, 5e-12]
