# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Find the next Jieqi after a fixed modern UTC instant and display it at UTC+8."""

from datetime import datetime, timedelta, timezone

import celestial_calendar as celestial

cutoff = datetime.fromisoformat("2026-10-28T19:35:37+00:00")
seconds = cutoff.hour * 3600 + cutoff.minute * 60 + cutoff.second
cutoff_utc = celestial.CivilDateTime(cutoff.year, cutoff.month, cutoff.day, seconds / 86400)
cutoff_jde = celestial.utc_to_jde(cutoff_utc)

events = (celestial.jieqi_moment(year, jieqi) for year in (cutoff.year, cutoff.year + 1) for jieqi in celestial.Jieqi)
# Jieqi moments are UT1, so compare them with the UTC cutoff only after conversion to TT.
candidates = ((celestial.ut1_to_jde(event.moment_ut1), event.jieqi) for event in events)
jde, jieqi = min((jde, jieqi) for jde, jieqi in candidates if jde > cutoff_jde)
utc = celestial.jde_to_utc(jde)

utc_display = datetime(utc.year, utc.month, utc.day, tzinfo=timezone.utc) + timedelta(days=utc.fraction)
local_display = utc_display.astimezone(timezone(timedelta(hours=8)))
print(celestial.jieqi_name(jieqi))
print("UTC:", utc_display.strftime("%Y-%m-%d %H:%M"))
print("UTC+8:", local_display.strftime("%Y-%m-%d %H:%M"))
