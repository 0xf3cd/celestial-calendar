# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import runpy
import sys
import json
import re
import shutil
import subprocess

from pathlib import Path
from types import ModuleType

import pytest


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize("defect", [None, "omitted", "failure"])
def test_python_query_main_invokes_every_scenario_and_propagates_failure(tmp_path, monkeypatch, defect):
  monkeypatch.setitem(sys.modules, "celestial_calendar", ModuleType("celestial_calendar"))
  smoke = ModuleType("smoke")
  smoke.raises = lambda *args: None
  monkeypatch.setitem(sys.modules, "smoke", smoke)
  source = (ROOT / "bindings/python/test/consumer/queries.py").read_text()
  if defect == "omitted":
    assert source.count("    scenario()\n") == 1
    source = source.replace("    scenario()\n", "    pass\n")
  path = tmp_path / "queries.py"
  path.write_text(source)
  module = runpy.run_path(str(path))
  main = module["main"]
  scenarios = [name for name in module if name.startswith("scenario_")]
  calls = []
  for name in scenarios:
    main.__globals__[name] = lambda name=name: calls.append(name)
  if defect == "failure":

    def fail():
      raise AssertionError("scenario failed")

    main.__globals__[scenarios[0]] = fail
    with pytest.raises(AssertionError, match="scenario failed"):
      main()
  else:
    main()
    if defect:
      assert calls != scenarios
    else:
      assert calls == scenarios and calls


@pytest.mark.parametrize("defect", [None, "omitted", "failure"])
def test_actual_js_chart_caller_invokes_every_scenario(tmp_path, defect):
  node = shutil.which("node")
  assert node is not None, "Node is required for the mock chart contract"
  path = ROOT / "bindings/javascript/test/support/chart_api.mjs"
  source = path.read_text()
  scenarios = re.findall(r"^      (\w+)\(\) \{", source, re.MULTILINE)
  assert scenarios
  source = re.sub(
    r"^      (\w+)\(\) \{",
    lambda match: match[0] + f"\n        globalThis.scenarioCalls.push({json.dumps(match[1])});",
    source,
    flags=re.MULTILINE,
  )
  source = source.replace('"./chart_replay.mjs"', json.dumps((path.parent / "chart_replay.mjs").as_uri()))
  source = source.replace(
    '"../../../../docs/examples/chart_snapshot/data.json"',
    json.dumps((ROOT / "docs/examples/chart_snapshot/data.json").as_uri()),
  )
  if defect == "omitted":
    source = source.replace("      scenario();", "      void scenario;")
  elif defect == "failure":
    source = source.replace("      dispatch() {", '      dispatch() { throw new Error("scenario failure");')
  probe = tmp_path / "chart_api.mjs"
  probe.write_text(source)
  driver = (
    'import assert from "node:assert/strict"; '
    f"import {{ testChartApi }} from {json.dumps(probe.as_uri())}; "
    "globalThis.scenarioCalls = []; "
    f"await testChartApi(null, new URL({json.dumps((ROOT / 'bindings/javascript/src/index.mjs').as_uri())}), "
    "{ native: false }); "
    f"assert.deepEqual(globalThis.scenarioCalls, {json.dumps(scenarios)}, 'scenario omission');"
  )
  result = subprocess.run([node, "--input-type=module", "-e", driver], capture_output=True, text=True)
  output = result.stdout + result.stderr
  if defect:
    assert result.returncode != 0 and f"scenario {'omission' if defect == 'omitted' else 'failure'}" in output, output
  else:
    assert result.returncode == 0, output


@pytest.mark.parametrize("defect", [None, "fraction", "field", "offset"])
def test_public_jieqi_decoder_sentinel_through_actual_mock_caller(tmp_path, defect):
  node = shutil.which("node")
  assert node is not None
  directory = ROOT / "bindings/javascript/src"
  source = (directory / "index.mjs").read_text()
  if defect:
    old, new = {
      "fraction": ("fraction: value.frac", "fraction: 0.5"),
      "field": ("month: value.m, day: value.d", "month: value.d, day: value.m"),
      "offset": (
        "const address = ptr + field.offset;",
        'const address = ptr + (field.name === "frac" ? 8 : field.offset);',
      ),
    }[defect]
    assert source.count(old) == 1
    source = source.replace(old, new)
  entry = tmp_path / "index.mjs"
  entry.write_text(source)
  for name in ("bindings.mjs", "validation.mjs"):
    shutil.copyfile(directory / name, tmp_path / name)
  test = ROOT / "bindings/javascript/test/node/fake_native_test.mjs"
  result = subprocess.run([node, str(test), str(entry)], capture_output=True, text=True)
  output = result.stdout + result.stderr
  if defect:
    assert result.returncode != 0 and "Jieqi public decoder" in output, output
  else:
    assert result.returncode == 0, output
