# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Non-declaration contracts, keyed by the exact C export name."""

EXPORT_SEMANTICS = {
  "celestial_last_error": {
    "protocol": {"kind": "borrowed-string", "error_reader": "message", "lifetime": "next-recording-call"},
    "recording": False,
  },
  "celestial_last_error_kind": {"protocol": {"error_reader": "kind"}, "recording": False},
  "celestial_ut1_to_jd": {"protocol": {}, "recording": True},
  "celestial_ut1_to_jde": {"protocol": {}, "recording": True},
  "celestial_jde_to_ut1": {"protocol": {}, "recording": True},
  "celestial_utc_to_jde": {"protocol": {}, "recording": True},
  "celestial_jde_to_utc": {"protocol": {}, "recording": True},
  "celestial_sun_apparent_geocentric_coord": {"protocol": {}, "recording": True},
  "celestial_moon_apparent_geocentric_coord": {"protocol": {}, "recording": True},
  "celestial_moon_illumination": {"protocol": {}, "recording": True},
  "celestial_moon_position_angle": {"protocol": {}, "recording": True},
  "celestial_moon_phase_moments": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "root_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": False,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
    },
    "recording": True,
  },
  "celestial_solar_lon_root_discriminant": {"protocol": {"kind": "companion-count"}, "recording": True},
  "celestial_solar_lon_roots": {
    "protocol": {
      "kind": "companion-fill",
      "count_export": "celestial_solar_lon_root_discriminant",
      "legal_empty": True,
      "written_policy": "min-total-capacity",
    },
    "recording": True,
  },
  "celestial_new_moons_after_jde": {
    "protocol": {
      "kind": "requested-fill",
      "capacity_parameter": "slot_count",
      "legal_empty": True,
      "written_policy": "exact-requested-count",
      "null_slots_requires_zero_capacity": True,
    },
    "recording": True,
  },
  "celestial_new_moons_in_year": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "root_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": False,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
    },
    "recording": True,
  },
  "celestial_equation_of_time": {"protocol": {}, "recording": True},
  "celestial_apparent_solar_time": {"protocol": {}, "recording": True},
  "celestial_local_apparent_sidereal_time": {"protocol": {}, "recording": True},
  "celestial_query_jieqi_moment": {"protocol": {}, "recording": True},
  "celestial_get_jieqi_name": {
    "protocol": {
      "kind": "sized-string",
      "buffer_parameter": "buf",
      "capacity_parameter": "buf_size",
      "size_includes_nul": True,
      "null_requires_zero_capacity": True,
      "truncates": True,
      "terminates": True,
    },
    "recording": True,
  },
  "celestial_get_supported_lunar_year_range": {"protocol": {}, "recording": True},
  "celestial_get_lunar_year_info": {"protocol": {}, "recording": True},
  "celestial_gregorian_to_lunar": {"protocol": {}, "recording": True},
  "celestial_lunar_to_gregorian": {"protocol": {}, "recording": True},
  "celestial_delta_t": {"protocol": {"identity": "resolved-explicit-model-key"}, "recording": True},
  "celestial_chart_snapshot": {
    "protocol": {
      "ownership": "by-value snapshot of {bodies.extent} bodies and {houses.cusps_deg.extent} cusps",
      "identity": "resolved-explicit-model-keys-and-per-body-source",
    },
    "recording": True,
  },
  "celestial_longitude_events": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "event_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": True,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
    },
    "recording": True,
  },
  "celestial_longitude_stations": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "event_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": True,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
    },
    "recording": True,
  },
  "celestial_longitude_difference_events": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "event_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": True,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
    },
    "recording": True,
  },
  "celestial_capabilities": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "record_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": False,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
      "operations": [
        "CELESTIAL_OPERATION_POSITION",
        "CELESTIAL_OPERATION_LONGITUDE_RATE",
        "CELESTIAL_OPERATION_EVENTS",
        "CELESTIAL_OPERATION_EQUATORIAL",
      ],
      "subject_policy": "concrete-target-or-all",
    },
    "recording": True,
  },
  "celestial_model_identities": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "record_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": False,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
      "identity": "kind-and-explicit-code",
    },
    "recording": True,
  },
  "celestial_model_identity_text": {
    "protocol": {
      "kind": "sized-string",
      "buffer_parameter": "buffer",
      "capacity_parameter": "capacity",
      "size_includes_nul": True,
      "null_requires_zero_capacity": True,
      "truncates": True,
      "terminates": True,
      "identity": "kind-and-explicit-code",
    },
    "recording": True,
  },
  "celestial_capability_text": {
    "protocol": {
      "kind": "sized-string",
      "buffer_parameter": "buffer",
      "capacity_parameter": "capacity",
      "size_includes_nul": True,
      "null_requires_zero_capacity": True,
      "truncates": True,
      "terminates": True,
      "subject_policy": "concrete-target-for-target-operations-all-for-nontarget-operations",
      "nontarget_operations": [
        "CELESTIAL_OPERATION_HOUSES",
        "CELESTIAL_OPERATION_CHART",
        "CELESTIAL_OPERATION_DELTA_T",
      ],
    },
    "recording": True,
  },
  "celestial_positions": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "record_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": False,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
      "selection_policy": "nonempty-distinct-targets-in-caller-order",
    },
    "recording": True,
  },
  "celestial_equatorial_positions": {
    "protocol": {
      "kind": "count-fill",
      "count_parameter": "record_count",
      "slots_parameter": "slots",
      "capacity_parameter": "slot_count",
      "legal_empty": False,
      "total_must_match": True,
      "written_policy": "min-total-capacity",
      "null_slots_requires_zero_capacity": True,
      "selection_policy": "nonempty-distinct-targets-in-caller-order",
    },
    "recording": True,
  },
  "celestial_house_query": {"protocol": {"identity": "resolved-explicit-model-keys"}, "recording": True},
  "celestial_house_capability": {"protocol": {}, "recording": True},
}
