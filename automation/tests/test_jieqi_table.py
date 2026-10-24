# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import copy
import ctypes
import hashlib
import json
import math

from datetime import datetime, timedelta
from types import SimpleNamespace

import pytest

from toolbox import jieqi_table
from automation import jieqi_table as checker


@pytest.mark.parametrize(
  ("fields", "jd", "display"),
  [
    ((2000, 1, 1, 0.5), 2451545.0, "2000-01-01T12:00:00.000"),
    ((2024, 6, 1, 0.5), 2460463.0, "2024-06-01T12:00:00.000"),
    ((401, 1, 1, 0.0), 1867522.5, "0401-01-01T00:00:00.000"),
  ],
)
def test_ut1_julian_day_and_unzoned_display(fields, jd, display):
  assert jieqi_table.ut1_fields(*fields) == (jd, display)


@pytest.mark.parametrize("year", [1961, 1972, 2016, 2050])
def test_each_era_stays_ut1_without_a_utc_claim(year):
  jd, display = jieqi_table.ut1_fields(year, 12, 31, 0.5)
  assert math.isfinite(jd)
  assert display == f"{year}-12-31T12:00:00.000"
  assert not display.endswith("Z")
  assert "No UTC conversion" in jieqi_table.TIMESCALE_NOTE


@pytest.mark.parametrize("fraction", [-1e-12, 1.0, 1e30, math.nan, math.inf])
def test_invalid_ut1_fields_fail(fraction):
  with pytest.raises(ValueError, match="fraction"):
    jieqi_table.ut1_fields(2024, 1, 1, fraction)


def test_display_truncates_without_midnight_rollover():
  _, display = jieqi_table.ut1_fields(2024, 1, 1, math.nextafter(1.0, 0.0))
  assert display == "2024-01-01T23:59:59.999"


def test_provenance_failure_precedes_native_loading(monkeypatch, tmp_path):
  artifact = tmp_path / "native-artifact"
  artifact.write_bytes(b"unverified")
  monkeypatch.setattr(jieqi_table, "find_shared_lib", lambda: artifact)

  def reject(*args):
    raise RuntimeError("missing provenance")

  def must_not_load(*args):
    raise AssertionError("native loading occurred before provenance verification")

  monkeypatch.setattr(jieqi_table, "verify_library", reject)
  monkeypatch.setattr(jieqi_table, "load_lib", must_not_load)
  with pytest.raises(RuntimeError, match="provenance"):
    jieqi_table.generate(2024, 2024)


@pytest.mark.parametrize("years", [(400, 2024), (2025, 2024), (2024, 10000)])
def test_invalid_year_window_precedes_loading(years):
  with pytest.raises(ValueError, match="year range"):
    jieqi_table.generate(*years)


@pytest.fixture
def model_library(monkeypatch):
  def install(defect=None):
    records = [(1, 0, 0xFFFFFFFF), (2, 5, 0xFFFFFFFF), (0, 17, 0)]
    if defect == "no-default":
      records[-1] = (0, 17, 0xFFFFFFFF)
    elif defect == "duplicate-default":
      records[0] = (0, 9, 0)

    def identities(kind, count, slots, capacity):
      assert kind == jieqi_table.MODEL_KIND_ALL
      ctypes.cast(count, ctypes.POINTER(ctypes.c_uint32)).contents.value = 0 if defect == "empty" else len(records)
      if capacity == 0:
        return 0
      for index, (model_kind, code, alias) in enumerate(records):
        slots[index].kind_code = model_kind
        slots[index].identity_code = code
        slots[index].default_alias_code = alias
      return len(records) - (defect == "short-fill")

    def text(operation, ordinal, output, capacity):
      assert operation == jieqi_table.CAPABILITY_OPERATION_MODEL_IDENTITY
      assert ordinal == 2
      value = b"fixture model 17"
      if defect == "missing-description":
        return 0
      if capacity:
        output.value = value
      return len(value) - (capacity > 0 and defect == "short-text")

    lib = SimpleNamespace(
      query_jieqi_moment=lambda *args: None,
      get_jieqi_name=lambda *args: True,
      celestial_model_identities=identities,
      celestial_capability_text=text,
      last_error=lambda: b"",
    )
    monkeypatch.setattr(jieqi_table.ctypes, "CDLL", lambda path: lib)
    return lib

  return install


def test_model_metadata_uses_the_loaded_registry_and_ordinal(model_library, tmp_path):
  expected = model_library()
  lib, models = jieqi_table.load_lib(tmp_path / "fixture")
  assert lib is expected
  assert models == {
    "delta_t": {"kind_code": 0, "identity_code": 17, "description": "fixture model 17"},
    "leap_seconds": None,
  }


@pytest.mark.parametrize(
  "defect", ["empty", "short-fill", "no-default", "duplicate-default", "missing-description", "short-text"]
)
def test_model_registry_failures_reject_export(model_library, tmp_path, defect):
  model_library(defect)
  with pytest.raises(RuntimeError):
    jieqi_table.load_lib(tmp_path / "fixture")


@pytest.mark.parametrize("failure", [None, "artifact-race", "receipt-race"])
def test_export_uses_private_verified_bytes_and_checks_again(monkeypatch, tmp_path, failure):
  artifact = tmp_path / "fixture.so"
  data = b"verified library bytes"
  artifact.write_bytes(data)
  receipt = {
    "artifact": {"sha256": hashlib.sha256(data).hexdigest()},
    "source": {"commit": None, "snapshot_sha256": "a" * 64},
  }
  if failure == "artifact-race":
    receipt["artifact"]["sha256"] = "0" * 64
  calls = []
  lib = object()
  models = {"delta_t": {"identity_code": 17}, "leap_seconds": None}

  def verify(*args):
    calls.append("verify")
    result = copy.deepcopy(receipt)
    if failure == "receipt-race" and calls.count("verify") == 2:
      result["artifact"]["sha256"] = "b" * 64
    return result

  def load(path):
    assert path != artifact and path.read_bytes() == data
    calls.append("load")
    return lib, models

  def entries(loaded, *args):
    assert loaded is lib
    calls.append("entries")
    return []

  monkeypatch.setattr(jieqi_table, "find_shared_lib", lambda: artifact)
  monkeypatch.setattr(jieqi_table, "verify_library", verify)
  monkeypatch.setattr(jieqi_table, "load_lib", load)
  monkeypatch.setattr(jieqi_table, "generate_entries", entries)
  if failure:
    with pytest.raises(RuntimeError, match="changed"):
      jieqi_table.generate(2024, 2024)
  else:
    assert jieqi_table.generate(2024, 2024)["models"] == models
  assert calls == (["verify"] if failure == "artifact-race" else ["verify", "load", "entries", "verify"])


@pytest.mark.parametrize(
  ("defect", "message"),
  [
    (None, None),
    ("schema", "schema @2"),
    ("leap-seconds", "leap-second conversion"),
    ("utc-alias", "UTC/Unix aliases"),
    ("jd-display", "iso_ut1 and jd_ut1 disagree"),
  ],
)
def test_table_checker_rejects_each_new_schema_defect(monkeypatch, capsys, defect, message):
  moments = [datetime(2024, 1, 1) + timedelta(days=index) for index in range(24)]
  doc = {
    "schema": "celestial-calendar/jieqi-table@2",
    "timescale": "UT1",
    "models": {"leap_seconds": None},
    "parameters": {"start_year": 2024, "end_year": 2024},
    "entries": [
      {
        "year": 2024,
        "idx": index,
        "name_zh": checker.JIEQI_NAMES_ZH[index],
        "jd_ut1": jieqi_table.ut1_fields(2024, 1, index + 1, 0.0)[0],
        "iso_ut1": moment.isoformat(timespec="milliseconds"),
      }
      for index, moment in enumerate(moments)
    ],
  }
  if defect == "schema":
    doc["schema"] = "celestial-calendar/jieqi-table@1"
  elif defect == "leap-seconds":
    doc["models"]["leap_seconds"] = "claimed conversion"
  elif defect == "utc-alias":
    doc["entries"][0]["unix_ms"] = 0
  elif defect == "jd-display":
    doc["entries"][0]["jd_ut1"] -= 0.5
  exporter = SimpleNamespace(
    DEFAULT_START_YEAR=2024,
    DEFAULT_END_YEAR=2024,
    JIEQI_PER_YEAR=24,
    generate=lambda *args: copy.deepcopy(doc),
    serialize=json.dumps,
  )
  common = SimpleNamespace(
    Jieqi=lambda index: index, jieqi_moment=lambda year, index: SimpleNamespace(moment=moments[index])
  )
  monkeypatch.setattr(checker, "load_exporter", lambda: exporter)
  monkeypatch.setattr(checker, "load_common", lambda: common)
  monkeypatch.setattr(checker, "HKO_ANCHORS", [])
  assert checker.check_jieqi_table() == (1 if defect else 0)
  if message:
    assert message in capsys.readouterr().out
