#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Angular-input bounds for the light-time spherical return before aberration/FK5."""

from fractions import Fraction as F

from .heliocentric_evaluation_error import angle_roundtrip_error
from .poisson_majorants import pi_interval
from .precession_evaluation_error import degree_normalized_storage_error
from .raw_evaluation_error import LIBM_ALLOWANCE
from .spherical_coordinate_majorants import dyadic_upper


def angular_allowance_from_errors(
  vector_error_au: F, horizontal_hypot_error_au: F, radial_lower_au: F, cylindrical_lower_au: F
) -> dict:
  """Transport vector/hypot allowances through separated source-return angles."""
  if min(vector_error_au, horizontal_hypot_error_au) < 0 or min(radial_lower_au, cylindrical_lower_au) <= 0:
    raise ValueError("Require nonnegative errors and positive radial/cylindrical lower bounds")
  if vector_error_au >= cylindrical_lower_au:
    raise ValueError("Longitude's complete perturbation segment is not certified away from the axis")
  latitude_input_error = vector_error_au + horizontal_hypot_error_au
  if latitude_input_error >= radial_lower_au:
    raise ValueError("Latitude's meridional perturbation segment is not certified away from zero")
  longitude_transport = vector_error_au / (cylindrical_lower_au - vector_error_au)
  latitude_transport = latitude_input_error / (radial_lower_au - latitude_input_error)
  longitude = longitude_transport + LIBM_ALLOWANCE + degree_normalized_storage_error(pi_interval()[1] + LIBM_ALLOWANCE)
  latitude = latitude_transport + LIBM_ALLOWANCE + angle_roundtrip_error(pi_interval()[1] / 2 + LIBM_ALLOWANCE)
  return {
    "longitude_circular_error_rad": dyadic_upper((longitude,))[0],
    "latitude_error_rad": dyadic_upper((latitude,))[0],
    "longitude_input_transport_rad": dyadic_upper((longitude_transport,))[0],
    "latitude_input_transport_rad": dyadic_upper((latitude_transport,))[0],
  }
