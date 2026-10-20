# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import pytest

from toolbox.make_abi_manifest import MANIFEST_PATHS, REPO_ROOT, build_manifest, render

COMMITTED = {view: (REPO_ROOT / relative).read_text(encoding="utf-8") for view, relative in MANIFEST_PATHS.items()}


def test_generator_reproduces_both_committed_manifests():
  for view, committed in COMMITTED.items():
    assert render(view) == committed


def test_the_two_views_are_not_identical():
  assert render("python") != render("javascript")
  python_manifest = build_manifest("python")
  javascript_manifest = build_manifest("javascript")
  assert python_manifest["schema"] != javascript_manifest["schema"]
  assert python_manifest["native_width_bits"]["pointer"] == 64
  assert javascript_manifest["wasm_width_bits"]["pointer"] == 32


def test_struct_kind_relabels_to_sret_in_the_javascript_view():
  python_struct = next(export for export in build_manifest("python")["exports"] if export["return"]["kind"] == "struct")
  javascript_export = next(
    export for export in build_manifest("javascript")["exports"] if export["name"] == python_struct["name"]
  )
  assert javascript_export["return"]["kind"] == "sret"
  assert javascript_export["protocol"]["kind"] == "sret"


def test_python_only_prose_stays_out_of_the_javascript_view():
  chart = next(export for export in build_manifest("javascript")["exports"] if export["name"] == "chart_snapshot_v1")
  assert "ownership" not in chart["protocol"]
  assert "presence" not in chart["protocol"]
  cusps = build_manifest("javascript")["layouts"]["ChartHousesV1"]["fields"][-1]
  assert cusps["name"] == "cusps_deg"
  assert "extent" not in cusps
  assert "stride" not in cusps


def test_a_source_edit_drifts_from_the_committed_manifest():
  from toolbox import make_abi_manifest

  original = make_abi_manifest.EXPORTS[0]["protocol"]["zero"]
  try:
    make_abi_manifest.EXPORTS[0]["protocol"]["zero"] = original + " (edited)"
    with pytest.raises(AssertionError):
      assert render("python") == COMMITTED["python"]
  finally:
    make_abi_manifest.EXPORTS[0]["protocol"]["zero"] = original
  assert render("python") == COMMITTED["python"]
