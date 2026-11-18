# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Replay the fixed position/event inputs through a provenance-verified native C library."""

import argparse
import ctypes
import importlib.util
import json
import struct
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parent.parent))

from automation.library_provenance import find_shared_lib, verify_library


ROOT = Path(__file__).resolve().parent.parent
FIXTURE = ROOT / "toolbox/position_events_golden.json"
OPERATIONS = {
  "position.ecliptic": ("celestial_positions", "CelestialPositionRecord"),
  "position.equatorial": ("celestial_equatorial_positions", "CelestialEquatorialPositionRecord"),
  "events.longitudeCrossings": ("celestial_longitude_events", "CelestialLongitudeEventRecord"),
  "events.longitudeStations": ("celestial_longitude_stations", "CelestialStationEventRecord"),
  "events.longitudeDifferences": ("celestial_longitude_difference_events", "CelestialLongitudeDifferenceEventRecord"),
}


def capture(library, abi, case):
  c_name, record_name = OPERATIONS[case["operation"]]
  record_type = getattr(abi, record_name)
  arguments = case.get("arguments")
  if "targets" in case:
    codes = (ctypes.c_uint32 * max(1, len(case["targets"])))(*case["targets"])
    arguments = [codes, len(case["targets"]), case["jde_tt"]]
  function = getattr(library, c_name)
  count = ctypes.c_uint32()
  if function(*arguments, ctypes.byref(count), None, 0) != 0:
    raise RuntimeError("Native count query wrote records")
  error = library.celestial_last_error_kind()
  if error and count.value:
    raise RuntimeError("Native failed count query returned a nonzero count")
  result = {"error": error, "count": count.value, "records": []}
  if error or not count.value:
    return result
  total = count.value
  slots = (record_type * total)()
  written = function(*arguments, ctypes.byref(count), slots, total)
  if library.celestial_last_error_kind() or written != total or count.value != total:
    raise RuntimeError("Native count/fill disagreement")
  for record in slots:
    fields = {}
    for name, _ in record._fields_:
      if name == "distance_au" and not record.present_fields & abi.C_CODES["CELESTIAL_PRESENT_DISTANCE"]:
        fields[name] = None
      else:
        value = getattr(record, name)
        if isinstance(value, float):
          value = f"0x{struct.unpack('<Q', struct.pack('<d', value))[0]:016x}"
        fields[name] = value
    result["records"].append(fields)
  return {**result, "written": written}


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--library", type=Path)
  parser.add_argument("--output", type=Path, default=FIXTURE)
  args = parser.parse_args()
  path = args.library or find_shared_lib()
  provenance = verify_library(path, ROOT)
  spec = importlib.util.spec_from_file_location(
    "position_events_abi", ROOT / "bindings/python/src/celestial_calendar/_abi.py"
  )
  abi = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(abi)
  library = ctypes.CDLL(str(path.resolve()))
  for name in [*(entry[0] for entry in OPERATIONS.values()), "celestial_last_error_kind"]:
    function = getattr(library, name)
    function.argtypes, function.restype = abi.BINDING_SPECS[name]
  fixture = json.loads(FIXTURE.read_text())
  for case in fixture["cases"]:
    inputs = {name: case[name] for name in ("name", "operation", "targets", "jde_tt", "arguments") if name in case}
    case.clear()
    case.update(inputs, **capture(library, abi, inputs))
  fixture["source_commit"] = provenance["source"]["commit"]
  fixture["library_sha256"] = provenance["artifact"]["sha256"]
  args.output.write_text(json.dumps(fixture, indent=2, allow_nan=False) + "\n")


if __name__ == "__main__":
  main()
