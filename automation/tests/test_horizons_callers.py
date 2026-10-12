# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import importlib
import math
import re
import sys

from pathlib import Path

import pytest
import requests


STATISTICS = Path(__file__).resolve().parents[2] / "statistics"
sys.path.insert(0, str(STATISTICS))
SHARED = importlib.import_module("horizons_common")
PLANET = importlib.import_module("planet_horizons_crawler")
PLUTO = importlib.import_module("pluto_horizons_crawler")
MOON = importlib.import_module("moon_horizons_crawler")
ILLUMINATION = importlib.import_module("moon_illumination_horizons_crawler")
SUN = importlib.import_module("sun_jieqi_golden_crawler")
EQUATORIAL = importlib.import_module("sun_equatorial_horizons_crawler")

IDENTITIES = {
  "10": ("Sun", "DE441"),
  "199": ("Mercury", "DE441"),
  "301": ("Moon", "DE441"),
  "999": ("Pluto", "plu060_merged"),
}
VALUES = {
  "ObsEcLon": "10.0000000",
  "ObsEcLat": "0.0000000",
  "delta": "1.250000000000",
  "deldot": "0.0123",
  "S-O-T": "10.0000",
  "Illu%": "25.00000",
  "phi": "120.0000",
  "X": "0.300000000000",
  "Y": "0.400000000000",
  "Z": "0.000000000000",
}
EPOCHS = (2451545.123456789, 2451546.987654321)
# These independent tokens describe the wire contract, not a production grid helper.
SIX_ECHOES = ("2451545.123457000", "2451546.987654000")
NINE_ECHOES = ("2451545.123456789", "2451546.987654321")
RANGE_ECHOES = (
  "2460676.500800741",
  "2460677.000800741",
  "2460677.500800741",
  "2460678.000800741",
  "2460678.500800741",
)
RANGE_LONS = ("350.0000000", "10.0000000", "30.0000000", "10.0000000", "350.0000000")


class Response:
  def __init__(self, text):
    self.text = text

  def raise_for_status(self):
    pass


def table(command, echoes, columns, *, scale="TT", values=None):
  name, source = IDENTITIES[command]
  prelude = (
    f"API VERSION: 1.2\nTarget body name: {name} ({command}) {{source: {source}}}\n"
    "Center body name: Earth (399) {source: DE441}\nCenter-site name: GEOCENTRIC\n"
    "Atmos refraction: NO (AIRLESS)\nCalendar mode   : Gregorian\n"
    "Units conversion: 1 au= 149597870.700 km\nRel. light bend : Sun, EARTH\n"
    "Observer-centered IAU76/80 ecliptic-of-date longitude and latitude\n"
    "with light-time, gravitational deflection of\nlight, and stellar aberrations.\n"
  )
  header = ["Date__(TT)", "Date_________JDTT", "", ""] if scale == "TT" else ["date", "JDTDB", "", ""]
  header += [*columns, ""]
  rows = []
  for index, echo in enumerate(echoes):
    row_values = values[index] if values else [VALUES[column] for column in columns]
    rows.append(", ".join(["2000-Jan-01", echo, "", "", *row_values, ""]))
  return prelude + ", ".join(header) + "\n$$SOE\n" + "\n".join(rows) + "\n$$EOE\n"


@pytest.fixture(autouse=True)
def no_network_or_wait(monkeypatch):
  def forbidden(*args, **kwargs):
    pytest.fail("unexpected live transport in an offline test")

  monkeypatch.setattr(SHARED.requests, "get", forbidden)
  monkeypatch.setattr(PLANET.time, "sleep", lambda _seconds: None)


def install_transport(monkeypatch, text):
  calls = []

  def get(url, *, params, timeout):
    calls.append((url, dict(params), timeout))
    return Response(text)

  monkeypatch.setattr(SHARED.requests, "get", get)
  return calls


def event_chain():
  start, stop = float(RANGE_ECHOES[0]), float(RANGE_ECHOES[-1])
  planet = PLANET.fetch_horizons_range(PLANET.TARGETS[0], start, stop, 720, "10,23,31,43")
  sun = PLANET.fetch_horizons_range(PLANET.SUN, start, stop, 720, "31")
  return PLANET.derive_events(PLANET.TARGETS[0], 2025, start, stop, planet, sun)


def install_range_transport(monkeypatch, damage=None):
  calls = []

  def get(url, *, params, timeout):
    calls.append((url, params, timeout))
    command = params["COMMAND"].strip("'")
    if command == "10":
      text = table(command, RANGE_ECHOES, ("ObsEcLon", "ObsEcLat"), values=[["0.0", "0.0"]] * 5)
    else:
      columns = ("Illu%", "S-O-T", "ObsEcLon", "ObsEcLat", "phi")
      text = table(
        command,
        RANGE_ECHOES,
        columns,
        values=[["25.00000", "10.0000", lon, "0.0000000", "120.0000"] for lon in RANGE_LONS],
      )
    return Response(damage(text) if damage else text)

  monkeypatch.setattr(SHARED.requests, "get", get)
  return calls


def alter_rows(text, alteration):
  prelude, data = text.split("$$SOE\n")
  rows, trailer = data.split("\n$$EOE")
  return prelude + "$$SOE\n" + "\n".join(alteration(rows.splitlines())) + "\n$$EOE" + trailer


def test_complete_range_keeps_the_event_set(monkeypatch):
  calls = install_range_transport(monkeypatch)
  events = event_chain()
  assert tuple(event.kind for event in events) == (
    "INFERIOR_CONJUNCTION",
    "GREATEST_EASTERN_ELONGATION",
    "STATION_RETROGRADE",
    "INFERIOR_CONJUNCTION",
  )
  assert tuple(event.jde for event in events) == pytest.approx(
    (2460676.750800741, 2460677.500800741, 2460677.500800741, 2460678.250800741),
    abs=1e-9,
    rel=0,
  )
  assert len(calls) == 2
  assert all(call[2] == 120 for call in calls)
  assert calls[0][1]["START_TIME"] == "'JD2460676.500800741'"
  assert calls[0][1]["STOP_TIME"] == "'JD2460678.500800741'"
  assert calls[0][1]["STEP_SIZE"] == "'720 m'"


@pytest.mark.parametrize(
  "damage",
  [
    lambda text: alter_rows(text, lambda rows: rows[1:]),
    lambda text: alter_rows(text, lambda rows: rows[:-1]),
    lambda text: alter_rows(text, lambda rows: rows[:2] + rows[3:]),
    lambda text: alter_rows(text, lambda rows: rows[:2] + [rows[1]] + rows[2:]),
    lambda text: alter_rows(text, lambda rows: [rows[1], rows[0], *rows[2:]]),
    lambda text: text.replace(RANGE_ECHOES[2], "2460677.600800741"),
    lambda text: text.replace("120.0000", "NaN"),
    lambda text: text.replace("25.00000", "inf"),
    lambda text: text.replace("10.0000,", "NaN,", 1),
    lambda text: text.replace("phi", "not-phi", 1),
  ],
  ids=[
    "missing-head",
    "missing-tail",
    "missing-middle",
    "duplicate",
    "unordered",
    "wrong-cadence",
    "nan-phase",
    "infinite-illumination",
    "nan-elongation",
    "missing-column",
  ],
)
def test_range_fetch_rejects_damage_before_event_derivation(monkeypatch, damage):
  install_range_transport(monkeypatch, damage)
  reached = []
  monkeypatch.setattr(PLANET, "derive_events", lambda *_args: reached.append(True))
  with pytest.raises(RuntimeError):
    event_chain()
  assert reached == []


@pytest.mark.parametrize("stop", [2460678.5008007407, 2460678.7], ids=["serialized-aligned", "nonaligned"])
def test_range_uses_serialized_endpoints_without_forcing_stop(monkeypatch, stop):
  calls = install_transport(monkeypatch, table("10", RANGE_ECHOES, ("ObsEcLon", "ObsEcLat")))
  rows = PLANET.fetch_horizons_range(PLANET.SUN, 2460676.5008007407, stop, 720, "31")
  assert tuple(row.jde for row in rows) == tuple(map(float, RANGE_ECHOES))
  assert calls[0][1]["START_TIME"] == "'JD2460676.500800741'"
  assert calls[0][1]["STOP_TIME"] == f"'JD{stop:.9f}'"


@pytest.mark.parametrize("step", [0, -1, 1.5, True])
def test_invalid_range_cadence_fails_before_transport(step):
  with pytest.raises(RuntimeError, match="cadence"):
    PLANET.fetch_horizons_range(PLANET.SUN, 2451545.0, 2451546.0, step, "31")


@pytest.mark.parametrize(
  ("start", "stop"),
  [
    (math.nan, 2451546.0),
    (2451545.0, math.inf),
    (2451546.0, 2451545.0),
  ],
)
def test_invalid_range_endpoints_fail_before_transport(start, stop):
  with pytest.raises(RuntimeError, match="endpoints must be finite and ordered"):
    PLANET.fetch_horizons_range(PLANET.SUN, start, stop, 720, "31")


def test_empty_grid_is_rejected_after_transport(monkeypatch):
  calls = install_transport(monkeypatch, table("10", (), ("ObsEcLon", "ObsEcLat")))
  with pytest.raises(RuntimeError, match="returned no rows"):
    PLANET.fetch_horizons_range(PLANET.SUN, 2451545.0, 2451546.0, 720, "31")
  assert len(calls) == 1


@pytest.mark.parametrize(
  ("text", "message"),
  [
    ("JDTT, value\n$$SOE\n2451545, 1, extra\n$$EOE", "row width differs"),
    ("no column line\n$$SOE\n2451545, 1\n$$EOE", "column header line not found"),
    ("JDTT, value\n$$EOE\n2451545, 1\n$$SOE", "invalid data markers"),
  ],
)
def test_csv_envelope_rejects_structural_damage(text, message):
  with pytest.raises(RuntimeError, match=message):
    SHARED.parse_csv_block(text)


def test_multiple_jd_columns_are_rejected():
  with pytest.raises(RuntimeError, match="JD column is not TT"):
    SHARED.column_indices(["JDTT", "anotherJDTT", "ObsEcLon"], ("ObsEcLon",), "TT")


CASES = {
  "planet": ("199", "20,23,31", "TT", 6, ("delta", "deldot", "S-O-T", "ObsEcLon", "ObsEcLat")),
  "pluto": ("999", "20,23,31", "TT", 6, ("delta", "deldot", "S-O-T", "ObsEcLon", "ObsEcLat")),
  "moon": ("301", "31,20", "TT", 6, ("ObsEcLon", "ObsEcLat", "delta", "deldot")),
  "illumination": ("301", "10", "TT", 6, ("Illu%",)),
  "sun-observer": ("10", "31,20", "TT", 9, ("ObsEcLon", "ObsEcLat", "delta", "deldot")),
  "sun-vectors": ("10", None, "TDB", 9, ("X", "Y", "Z")),
}


def fetch_case(case):
  if case == "planet":
    return PLANET.fetch_horizons(PLANET.TARGETS[0], EPOCHS)
  if case == "pluto":
    return PLUTO.fetch_horizons(PLUTO.PLUTO, EPOCHS)
  if case == "moon":
    return MOON.fetch_horizons(list(EPOCHS))
  if case == "illumination":
    return ILLUMINATION.fetch_illumination(list(EPOCHS))
  if case == "sun-observer":
    return SUN.fetch_observer("10", list(EPOCHS))
  if case == "sun-vectors":
    return SUN.fetch_vectors_r("10", list(EPOCHS))
  raise AssertionError(case)


@pytest.mark.parametrize("case", CASES)
def test_callers_keep_their_wire_and_quantity_semantics(monkeypatch, case):
  command, quantities, scale, digits, columns = CASES[case]
  echoes = SIX_ECHOES if digits == 6 else NINE_ECHOES
  calls = install_transport(monkeypatch, table(command, echoes, columns, scale=scale))
  result = fetch_case(case)
  assert len(result) == 2
  url, params, timeout = calls[0]
  assert url == "https://ssd.jpl.nasa.gov/api/horizons.api"
  assert timeout == (120 if case.startswith("sun-") else 60)
  assert params["COMMAND"] == f"'{command}'"
  assert params["CENTER"] == "'500@399'"
  assert params["TLIST"] == (
    "'2451545.123457' '2451546.987654'" if digits == 6 else "'2451545.123456789' '2451546.987654321'"
  )
  assert params["TLIST_TYPE"] == "'JD'"
  if quantities:
    assert params["TIME_TYPE"] == "'TT'"
    assert params["QUANTITIES"] == f"'{quantities}'"
    assert params["EPHEM_TYPE"] == "'OBSERVER'"
    assert params["CAL_FORMAT"] == "'BOTH'"
  else:
    assert params["EPHEM_TYPE"] == "'VECTORS'"
    assert params["VEC_TABLE"] == "'1'"
    assert params["OUT_UNITS"] == "'AU-D'"
    assert "TIME_TYPE" not in params  # default TDB, no observer TT policy leaks in
    assert "VEC_CORR" not in params  # default NONE: geometric, not apparent range
    assert tuple(result) == EPOCHS
    assert tuple(result.values()) == (0.5, 0.5)
  if case == "moon":
    assert result[EPOCHS[0]]["r_km"] == 1.25 * 149597870.700
    assert result[EPOCHS[0]]["deldot"] == 0.0123
  if case == "illumination":
    assert result == [0.25, 0.25]  # no quantity-31 columns were supplied
  if case == "sun-observer":
    assert tuple(result) == EPOCHS
    assert set(result[EPOCHS[0]]) == {"date", "lon", "lat", "deldot"}
  if case in {"planet", "pluto"}:
    assert result[0].range_au == 1.25
    assert math.isnan(result[0].phase_angle_deg)


@pytest.mark.parametrize("case", CASES)
@pytest.mark.parametrize(
  "damage",
  [
    "truncated",
    "duplicate",
    "unordered",
    "wrong-epoch",
    "missing-column",
    "nonfinite",
    "markers",
    "source",
    "time-scale",
  ],
)
def test_callers_reject_incomplete_or_invalid_responses(monkeypatch, case, damage):
  command, _quantities, scale, digits, columns = CASES[case]
  echoes = SIX_ECHOES if digits == 6 else NINE_ECHOES
  text = table(command, echoes, columns, scale=scale)
  if damage == "truncated":
    text = alter_rows(text, lambda rows: rows[:1])
  elif damage == "duplicate":
    text = text.replace(echoes[1], echoes[0])
  elif damage == "unordered":
    text = alter_rows(text, lambda rows: list(reversed(rows)))
  elif damage == "wrong-epoch":
    text = text.replace(echoes[1], "2451547.000000000")
  elif damage == "missing-column":
    text = text.replace(columns[0], "not-the-column", 1)
  elif damage == "nonfinite":
    text = text.replace(VALUES[columns[-1]], "NaN")
  elif damage == "markers":
    text = text.replace("$$SOE", "$$SOE\n$$SOE")
  elif damage == "source":
    text = text.replace("DE441", "DE442").replace("plu060_merged", "DE442")
  elif damage == "time-scale":
    text = text.replace("JDTT", "JDTDB") if scale == "TT" else text.replace("JDTDB", "JDTT")
  install_transport(monkeypatch, text)
  with pytest.raises(RuntimeError):
    fetch_case(case)


@pytest.mark.parametrize("column", ("delta", "deldot", "S-O-T", "ObsEcLon", "ObsEcLat", "Illu%", "phi"))
@pytest.mark.parametrize("value", ["NaN", "inf", "-inf", "n.a."])
def test_every_requested_planet_quantity_is_finite(monkeypatch, column, value):
  columns = ("delta", "deldot", "S-O-T", "ObsEcLon", "ObsEcLat", "Illu%", "phi")
  rows = [[value if name == column else VALUES[name] for name in columns] for _ in SIX_ECHOES]
  install_transport(monkeypatch, table("199", SIX_ECHOES, columns, values=rows))
  with pytest.raises(RuntimeError):
    PLANET.fetch_horizons(PLANET.TARGETS[0], EPOCHS, "10,20,23,31,43")


@pytest.mark.parametrize("case", ["planet", "pluto", "moon", "illumination", "sun-observer", "sun-vectors"])
def test_batched_callers_have_complete_chunk_boundaries(monkeypatch, case):
  epochs = tuple(2451545.5 + day for day in range(81))
  calls = []
  command, _quantities, scale, digits, columns = CASES[case]

  def get(url, *, params, timeout):
    calls.append(params)
    # An independent requested slice fixes the expected rows; do not derive it from the production TLIST.
    chunk = epochs[(len(calls) - 1) * 40 : len(calls) * 40]
    assert params["TLIST"] == " ".join(f"'{epoch:.{digits}f}'" for epoch in chunk)
    return Response(table(command, tuple(f"{epoch:.9f}" for epoch in chunk), columns, scale=scale))

  monkeypatch.setattr(SHARED.requests, "get", get)
  if case == "planet":
    rows = PLANET.fetch_horizons_batched(PLANET.TARGETS[0], epochs)
  elif case == "pluto":
    rows = PLUTO.fetch_horizons_batched(PLUTO.PLUTO, epochs)
  elif case == "moon":
    rows = MOON.fetch_horizons(list(epochs))
  elif case == "illumination":
    rows = ILLUMINATION.fetch_illumination(list(epochs))
  elif case == "sun-observer":
    rows = SUN.fetch_observer("10", list(epochs))
  else:
    rows = SUN.fetch_vectors_r("10", list(epochs))
  assert len(rows) == 81
  assert len(calls) == 3


@pytest.mark.parametrize(
  ("count", "expected"),
  [
    (0, []),
    (1, [("request", 1)]),
    (40, [("request", 40)]),
    (41, [("request", 40), ("sleep", 0.15), ("request", 1)]),
    (81, [("request", 40), ("sleep", 0.15), ("request", 40), ("sleep", 0.15), ("request", 1)]),
  ],
)
def test_moon_batches_only_pace_between_requests(monkeypatch, count, expected):
  epochs = [2451545.5 + day for day in range(count)]
  events = []
  requested = 0

  def get(url, *, params, timeout):
    nonlocal requested
    chunk = epochs[requested : requested + 40]
    requested += len(chunk)
    events.append(("request", len(chunk)))
    assert params["TLIST"] == " ".join(f"'{epoch:.6f}'" for epoch in chunk)
    assert timeout == 60
    return Response(table("301", tuple(f"{epoch:.9f}" for epoch in chunk), ("ObsEcLon", "ObsEcLat", "delta", "deldot")))

  monkeypatch.setattr(SHARED.requests, "get", get)
  monkeypatch.setattr(MOON.time, "sleep", lambda seconds: events.append(("sleep", seconds)))
  rows = MOON.fetch_horizons(epochs)
  assert tuple(rows) == tuple(epochs)
  assert events == expected


@pytest.mark.parametrize("failure_at", [1, 2])
def test_moon_batch_failure_stops_without_retry_or_trailing_pacing(monkeypatch, failure_at):
  epochs = [2451545.5 + day for day in range(81)]
  events = []
  calls = 0

  def get(url, *, params, timeout):
    nonlocal calls
    calls += 1
    events.append(("request", calls))
    if calls == failure_at:
      raise requests.ConnectionError("Moon batch transport control")
    chunk = epochs[(calls - 1) * 40 : calls * 40]
    return Response(table("301", tuple(f"{epoch:.9f}" for epoch in chunk), ("ObsEcLon", "ObsEcLat", "delta", "deldot")))

  monkeypatch.setattr(SHARED.requests, "get", get)
  monkeypatch.setattr(MOON.time, "sleep", lambda seconds: events.append(("sleep", seconds)))
  with pytest.raises(requests.ConnectionError, match="Moon batch transport control"):
    MOON.fetch_horizons(epochs)
  assert calls == failure_at
  assert events == ([("request", 1)] if failure_at == 1 else [("request", 1), ("sleep", 0.15), ("request", 2)])


def equatorial_response():
  expected = EQUATORIAL.stored_rows()
  columns = ("R.A.__(a-app)", "DEC___(a-app)")
  echoes = ["2432253.451627002", *tuple(expected)[1:]]
  return table("10", echoes, columns, values=list(expected.values())), expected


def test_sun_equatorial_transport_preserves_all_stored_digits(monkeypatch, capsys):
  text, expected = equatorial_response()
  calls = install_transport(monkeypatch, text)
  EQUATORIAL.main()
  output = capsys.readouterr()
  assert "all 42 Horizons rows match every stored digit" in output.err
  assert output.out.splitlines()[1:] == [
    f"    {{ {jde:>14}, {{ {ra:>13}, {dec:>13} }} }}," for jde, (ra, dec) in expected.items()
  ]
  assert calls == [
    (
      "https://ssd.jpl.nasa.gov/api/horizons.api",
      {
        "format": "text",
        "COMMAND": "'10'",
        "OBJ_DATA": "'NO'",
        "MAKE_EPHEM": "'YES'",
        "EPHEM_TYPE": "'OBSERVER'",
        "CENTER": "'500@399'",
        "TLIST": " ".join(f"'{jde}'" for jde in expected),
        "TLIST_TYPE": "'JD'",
        "TIME_TYPE": "'TT'",
        "QUANTITIES": "'2'",
        "ANG_FORMAT": "'DEG'",
        "EXTRA_PREC": "'YES'",
        "CAL_FORMAT": "'BOTH'",
        "CSV_FORMAT": "'YES'",
      },
      60,
    )
  ]


@pytest.mark.parametrize("damage", ["digit", "missing", "duplicate", "unordered", "nan", "inf", "column", "scale"])
def test_sun_equatorial_replay_rejects_damage(monkeypatch, damage):
  text, expected = equatorial_response()
  if damage == "digit":
    ra = next(iter(expected.values()))[0]
    text = text.replace(ra, ra[:-1] + str((int(ra[-1]) + 1) % 10), 1)
  elif damage == "missing":
    text = alter_rows(text, lambda rows: rows[:-1])
  elif damage == "duplicate":
    text = alter_rows(text, lambda rows: [rows[0], rows[0], *rows[2:]])
  elif damage == "unordered":
    text = alter_rows(text, lambda rows: list(reversed(rows)))
  elif damage in {"nan", "inf"}:
    text = text.replace(next(iter(expected.values()))[0], damage, 1)
  elif damage == "column":
    text = text.replace("R.A.__(a-app)", "R.A.__(ICRF)")
  else:
    text = text.replace("JDTT", "JDTDB")
  install_transport(monkeypatch, text)
  with pytest.raises(RuntimeError):
    EQUATORIAL.main()


@pytest.mark.parametrize("case", ["planet", "pluto", "moon", "illumination", "equatorial"])
def test_single_attempt_callers_do_not_inherit_sun_retries(monkeypatch, case):
  calls = []

  def get(*args, **kwargs):
    calls.append(True)
    raise requests.ConnectionError("offline transport control")

  monkeypatch.setattr(SHARED.requests, "get", get)
  with pytest.raises(requests.ConnectionError):
    EQUATORIAL.fetch_horizons() if case == "equatorial" else fetch_case(case)
  assert len(calls) == 1


@pytest.mark.parametrize("status", [400, 404, 429, 500, 502])
def test_sun_retry_policy_remains_local(monkeypatch, status):
  calls, sleeps = [], []
  response = requests.Response()
  response.status_code = status

  def get(*args, **kwargs):
    calls.append(True)
    raise requests.HTTPError("offline status control", response=response)

  monkeypatch.setattr(SHARED.requests, "get", get)
  monkeypatch.setattr(SUN.time, "sleep", sleeps.append)
  with pytest.raises(requests.HTTPError if status < 500 else RuntimeError):
    SUN.fetch_observer("10", list(EPOCHS))
  assert len(calls) == (1 if status < 500 else 4)
  assert sleeps == ([] if status < 500 else [5.0, 10.0, 15.0])


def test_sun_retries_connection_failure_but_not_a_source_failure(monkeypatch):
  calls = []

  def get(*args, **kwargs):
    calls.append(True)
    if len(calls) < 3:
      raise requests.ConnectionError("offline connection control")
    return Response(table("10", NINE_ECHOES, ("ObsEcLon", "ObsEcLat", "delta", "deldot")))

  monkeypatch.setattr(SHARED.requests, "get", get)
  assert len(SUN.fetch_observer("10", list(EPOCHS))) == 2
  assert len(calls) == 3
  calls.clear()
  invalid_calls = install_transport(monkeypatch, "{source: DE442}")
  with pytest.raises(RuntimeError, match="DE441"):
    SUN.fetch_observer("10", list(EPOCHS))
  assert len(invalid_calls) == 1


@pytest.mark.parametrize(
  ("case", "column"),
  [
    ("moon", "ObsEcLon"),
    ("moon", "ObsEcLat"),
    ("moon", "delta"),
    ("moon", "deldot"),
    ("sun-observer", "ObsEcLon"),
    ("sun-observer", "ObsEcLat"),
    ("sun-observer", "delta"),
    ("sun-observer", "deldot"),
    ("sun-vectors", "X"),
    ("sun-vectors", "Y"),
    ("sun-vectors", "Z"),
    ("illumination", "Illu%"),
  ],
)
def test_required_finite_columns_in_other_callers(monkeypatch, case, column):
  command, _quantities, scale, digits, columns = CASES[case]
  echoes = SIX_ECHOES if digits == 6 else NINE_ECHOES
  values = [["NaN" if name == column else VALUES[name] for name in columns] for _ in echoes]
  install_transport(monkeypatch, table(command, echoes, columns, scale=scale, values=values))
  with pytest.raises(RuntimeError):
    fetch_case(case)


@pytest.mark.parametrize("caller", ["planet", "pluto", "emit-geometry"])
@pytest.mark.parametrize("drift", [None, "separation", "fraction"])
def test_geometry_callers_use_the_shared_checks_through_transport(monkeypatch, capsys, caller, drift):
  calls = []

  def get(url, *, params, timeout):
    calls.append(params)
    command = params["COMMAND"].strip("'")
    if command == "10":
      columns = ("delta", "deldot", "ObsEcLon", "ObsEcLat") if caller == "emit-geometry" else ("ObsEcLon", "ObsEcLat")
      values = [[VALUES[name] if name in {"delta", "deldot"} else "0.0" for name in columns]] * 2
      return Response(table(command, SIX_ECHOES, columns, values=values))
    columns = ("delta", "deldot", "S-O-T", "ObsEcLon", "ObsEcLat", "Illu%", "phi")
    text = table(command, SIX_ECHOES, columns)
    if drift == "separation":
      text = text.replace("10.0000,", "20.0000,")
    elif drift == "fraction":
      text = text.replace("25.00000", "50.00000")
    return Response(text)

  monkeypatch.setattr(SHARED.requests, "get", get)
  monkeypatch.setattr(PLUTO, "RETAINED_POSITION_EPOCHS", EPOCHS)
  monkeypatch.setattr(PLANET, "EPOCHS", EPOCHS)
  monkeypatch.setattr(PLANET, "TARGETS", (PLANET.TARGETS[0],))
  events = tuple(PLANET.EventRow("MERCURY", 2000, "INFERIOR_CONJUNCTION", epoch) for epoch in EPOCHS)

  def run():
    if caller == "emit-geometry":
      return PLANET.emit_geometry_rows()
    return PLUTO.retained_geometry() if caller == "pluto" else PLANET.annotate_events(PLANET.TARGETS[0], events)

  if drift:
    with pytest.raises(RuntimeError, match="quantity 23" if drift == "separation" else "quantity 10"):
      run()
  else:
    result = run()
    if caller == "emit-geometry":
      output = capsys.readouterr().out
      assert output.count("Planet::MERCURY") == 2
      assert "10.0000000" in output and "120.0000" in output and "0.25000000" in output
    elif caller == "pluto":
      assert len(result) == 2
      assert result[0]["derived_separation_deg"] == pytest.approx(10.0, abs=1e-12)
      assert result[0]["illuminated_fraction"] == 0.25
    else:
      assert len(result) == 2
      assert result[0].elongation_deg == pytest.approx(10.0, abs=1e-12)
      assert result[0].illuminated_fraction == 0.25
  assert len(calls) == 2


@pytest.mark.parametrize("slot", range(4))
@pytest.mark.parametrize("value", [math.nan, math.inf])
def test_separation_rejects_nonfinite_coordinate_components(slot, value):
  coordinates = [0.0, 0.0, 10.0, 0.0]
  coordinates[slot] = value
  with pytest.raises(RuntimeError, match="requires finite coordinates"):
    SHARED.angular_separation_deg(tuple(coordinates[:2]), tuple(coordinates[2:]))


@pytest.mark.parametrize("kind", ["elongation", "phase", "illumination"])
def test_shared_geometry_rejects_nonfinite_inputs(kind):
  if kind == "elongation":
    with pytest.raises(RuntimeError, match="quantity 23"):
      SHARED.checked_separation((0.0, 0.0), (10.0, 0.0), math.nan, "control")
  else:
    phase, fraction = (math.nan, 0.25) if kind == "phase" else (120.0, math.nan)
    with pytest.raises(RuntimeError, match="quantity 10"):
      SHARED.require_illumination(phase, fraction, "control")


def test_shared_geometry_has_independent_absolute_anchors():
  assert SHARED.angular_separation_deg((359.0, 0.0), (1.0, 0.0)) == pytest.approx(2.0, abs=1e-12)
  assert SHARED.angular_separation_deg((0.0, 0.0), (0.0, 90.0)) == pytest.approx(90.0, abs=1e-12)
  assert SHARED.checked_separation((0.0, 0.0), (10.0, 0.0), 10.00019, "control") == pytest.approx(10.0)
  SHARED.require_illumination(120.0, 0.2500019, "control")
  with pytest.raises(RuntimeError, match="quantity 23"):
    SHARED.checked_separation((0.0, 0.0), (10.0, 0.0), 10.00021, "control")
  with pytest.raises(RuntimeError, match="quantity 10"):
    SHARED.require_illumination(120.0, 0.2500021, "control")


def test_shared_tlist_quotes_each_token():
  assert SHARED.tlist(("2451545.000000", "2451546.123456789")) == "'2451545.000000' '2451546.123456789'"
  assert re.findall(r"'([^']+)'", SHARED.tlist(("1", "2", "3"))) == ["1", "2", "3"]
