/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "lunar/algo3.hpp"

// Independent external-oracle golden for algo3 baked years (#70 §2 / D-P).
//
// ## Source
// ytliu0 / ChineseCalendar (廖育棟 / Yuk-Tung Liu), commit
//   d6aae82b63b79a6f8659ea3e064024b7d8ac3077
// file `src/calendarData.js` (695460 bytes, md5 6c9649f384d178918d9cb4618f7d3e98),
// function `ChineseToGregorian()` — the main year table.
// License of that repository: GPLv3 (same family as this repo's GPLv3+). This comment
// records the fact; it is not a legal opinion about combining the works.
// Collected 2026-08-05. Frozen sample of the 114-row main table:
//   vault `attachments/2026-08-05-P5b-golden-前期工/golden-采样草案.md`
//   (in-repo copy under `.review/style-arch/golden-pre/` for CLI readers).
// Regenerator / re-verifier: `statistics/algo3_ytliu0_golden.py`
//   (`emit-cpp`, `compare-all`, and the 1901–1929 near-midnight scan).
//
// ## Row set
// - 114 rows from the frozen sample (endpoint substitute **1603** — year 1600 itself is
//   a Ming 订正 year and is only in the sample as 备查, not in this table).
// - +1 row for **2099** (W-A2, 2026-08-05): upper algo1/algo3 seam next to 2100, decoded
//   from the same commit's calendarData.js at byte offset noted on the row. No other
//   re-sampling is in scope.
// Coverage vs algo3's 600 baked years: 115/600 ≈ 19%. By window:
//   69/301 in 1600–1900 (incl. lower seam 1900/1901), **2/199** in 1901–2099
//   (only the two seams 1901 and 2099 — the HKO bulk has almost no external pin),
//   45/100 in 2100–2199. Comments must NOT claim "algo3 already has a full external oracle".
//
// ## Independence (write per segment; do not collapse into one sentence)
// - 2100–2199 (45 rows): both sides are modern computation —
//   ytliu0 DE441+SM16 vs this repo VSOP87/ELP2000+algo5. Fully independent chains.
// - Qing 1645–1900 sample rows: ytliu0's pipeline is "modern compute then correct against
//   the published almanac"; years that needed no correction are the modern-compute edge
//   and may be compared for independence against algo3.
// - Ming 1600–1644 six rows in this table (1603/1607/1618/1625/1633/1639): labelled
//   **巧合全等** — ytliu0's base material is 张培瑜 historical reconstruction, NOT DE441.
//   Do not put these six under the DE441-independence sentence.
//
// ## Sampling honesty
// Rows were selected only among years where the two chains already agreed on the four
// structural fields at collection time. Under today's construction the table is therefore
// green by design; what it pins is *future drift*, not "algo3 is correct today".
// Qing "avoid 修正年" operational definition = avoid the 28 years where algo3 disagreed
// on those fields (there is no published full 修正年 roster to cite).
// Residual full-range mismatches at collection: 52 of 600, bucketed by era
// (Qing 28 + Ming 22 + 2057/2097) — case-by-case root causes are NOT claimed here.
//
// ## Seams / endpoints covered
// 1603 (endpoint substitute) · 1900/1901 (algo1-source switch) · 2099/2100 (upper seam;
// 2099 via W-A2) · 2199 · #64 re-bake years 2133/2165/2172.
//
// Integer civil-day fields: zero tolerance (`ASSERT_EQ`).

namespace calendar::lunar::algo3::test {

namespace {

struct Ytliu0Row {
  int32_t year;
  std::chrono::year_month_day first_day;
  uint8_t leap_month;
  std::vector<uint32_t> month_lengths;
};

// 115 rows: frozen 114 + W-A2 2099. `js@N` = byte offset into calendarData.js at the
// pinned commit (provenance for the pre-work package / the 2099 decode).
const std::vector<Ytliu0Row> kYtliu0Rows {
  { 1603, std::chrono::year { 1603 } / 2 / 11, 0, { 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@103230 total=354
  { 1607, std::chrono::year { 1607 } / 1 / 28, 6, { 29, 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30 } },  // js@103474 total=384
  { 1618, std::chrono::year { 1618 } / 1 / 26, 4, { 30, 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 } },  // js@104147 total=384
  { 1625, std::chrono::year { 1625 } / 2 / 7, 0, { 30, 29, 29, 30, 29, 30, 30, 29, 30, 30, 29, 30 } },  // js@104577 total=355
  { 1633, std::chrono::year { 1633 } / 2 / 8, 0, { 30, 29, 30, 30, 29, 30, 29, 30, 30, 29, 30, 29 } },  // js@105067 total=355
  { 1639, std::chrono::year { 1639 } / 2 / 3, 0, { 30, 29, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30 } },  // js@105433 total=354
  { 1647, std::chrono::year { 1647 } / 2 / 5, 0, { 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30, 30 } },  // js@105923 total=354
  { 1652, std::chrono::year { 1652 } / 2 / 9, 0, { 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 30, 29 } },  // js@106228 total=355
  { 1656, std::chrono::year { 1656 } / 1 / 26, 5, { 30, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@106471 total=384
  { 1660, std::chrono::year { 1660 } / 2 / 11, 0, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29 } },  // js@106716 total=354
  { 1666, std::chrono::year { 1666 } / 2 / 4, 0, { 30, 29, 30, 30, 29, 30, 29, 29, 30, 29, 30, 29 } },  // js@107083 total=354
  { 1669, std::chrono::year { 1669 } / 2 / 1, 0, { 29, 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 29 } },  // js@107266 total=354
  { 1674, std::chrono::year { 1674 } / 2 / 6, 0, { 29, 30, 30, 29, 30, 29, 29, 30, 29, 29, 30, 30 } },  // js@107571 total=354
  { 1678, std::chrono::year { 1678 } / 1 / 23, 3, { 29, 30, 29, 29, 30, 30, 29, 30, 30, 29, 30, 29, 30 } },  // js@107814 total=384
  { 1681, std::chrono::year { 1681 } / 2 / 18, 0, { 30, 29, 30, 29, 29, 30, 29, 29, 30, 30, 30, 29 } },  // js@107999 total=354
  { 1688, std::chrono::year { 1688 } / 2 / 2, 0, { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 } },  // js@108426 total=354
  { 1692, std::chrono::year { 1692 } / 2 / 17, 0, { 30, 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30 } },  // js@108670 total=354
  { 1696, std::chrono::year { 1696 } / 2 / 3, 0, { 29, 30, 29, 30, 29, 30, 29, 30, 30, 30, 29, 30 } },  // js@108914 total=355
  { 1700, std::chrono::year { 1700 } / 2 / 19, 0, { 30, 29, 30, 29, 29, 30, 29, 29, 30, 29, 30, 30 } },  // js@109159 total=354
  { 1705, std::chrono::year { 1705 } / 1 / 25, 4, { 29, 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 30, 29 } },  // js@109463 total=384
  { 1710, std::chrono::year { 1710 } / 1 / 30, 7, { 29, 30, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29 } },  // js@109769 total=383
  { 1713, std::chrono::year { 1713 } / 1 / 26, 5, { 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 30, 29, 29 } },  // js@109952 total=384
  { 1717, std::chrono::year { 1717 } / 2 / 11, 0, { 30, 30, 29, 29, 30, 29, 29, 30, 29, 30, 30, 29 } },  // js@110197 total=354
  { 1722, std::chrono::year { 1722 } / 2 / 16, 0, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29 } },  // js@110503 total=354
  { 1725, std::chrono::year { 1725 } / 2 / 13, 0, { 29, 30, 29, 30, 29, 29, 30, 29, 30, 30, 29, 30 } },  // js@110686 total=354
  { 1730, std::chrono::year { 1730 } / 2 / 17, 0, { 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29, 30 } },  // js@110992 total=355
  { 1734, std::chrono::year { 1734 } / 2 / 4, 0, { 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30, 30 } },  // js@111236 total=354
  { 1737, std::chrono::year { 1737 } / 1 / 31, 9, { 29, 30, 30, 29, 30, 29, 30, 29, 30, 29, 29, 30, 30 } },  // js@111419 total=384
  { 1741, std::chrono::year { 1741 } / 2 / 16, 0, { 29, 30, 29, 29, 30, 29, 30, 30, 29, 30, 30, 29 } },  // js@111664 total=354
  { 1745, std::chrono::year { 1745 } / 2 / 1, 0, { 30, 30, 30, 29, 30, 29, 29, 30, 29, 29, 30, 30 } },  // js@111908 total=355
  { 1749, std::chrono::year { 1749 } / 2 / 17, 0, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 30, 29, 30 } },  // js@112153 total=355
  { 1752, std::chrono::year { 1752 } / 2 / 15, 0, { 30, 29, 30, 29, 29, 29, 30, 29, 30, 30, 29, 30 } },  // js@112336 total=354
  { 1756, std::chrono::year { 1756 } / 1 / 31, 9, { 30, 30, 29, 30, 29, 30, 30, 29, 30, 29, 29, 30, 29 } },  // js@112580 total=384
  { 1761, std::chrono::year { 1761 } / 2 / 5, 0, { 30, 29, 30, 29, 29, 29, 30, 29, 30, 29, 30, 30 } },  // js@112886 total=354
  { 1765, std::chrono::year { 1765 } / 1 / 21, 2, { 30, 29, 30, 30, 29, 30, 29, 30, 30, 29, 29, 30, 29 } },  // js@113129 total=384
  { 1769, std::chrono::year { 1769 } / 2 / 7, 0, { 29, 30, 29, 29, 29, 30, 29, 30, 29, 30, 30, 30 } },  // js@113374 total=354
  { 1773, std::chrono::year { 1773 } / 1 / 23, 3, { 29, 30, 30, 29, 30, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@113617 total=384
  { 1776, std::chrono::year { 1776 } / 2 / 19, 0, { 30, 29, 30, 29, 29, 30, 30, 29, 30, 30, 29, 30 } },  // js@113803 total=355
  { 1782, std::chrono::year { 1782 } / 2 / 12, 0, { 30, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29, 30 } },  // js@114169 total=355
  { 1786, std::chrono::year { 1786 } / 1 / 30, 7, { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29, 30 } },  // js@114412 total=384
  { 1792, std::chrono::year { 1792 } / 1 / 24, 4, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29, 30 } },  // js@114778 total=384
  { 1795, std::chrono::year { 1795 } / 1 / 21, 2, { 29, 30, 29, 29, 30, 29, 30, 29, 30, 29, 30, 30, 30 } },  // js@114961 total=384
  { 1800, std::chrono::year { 1800 } / 1 / 25, 4, { 30, 29, 30, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29 } },  // js@115266 total=384
  { 1805, std::chrono::year { 1805 } / 1 / 31, 6, { 29, 30, 29, 30, 29, 29, 29, 30, 29, 30, 30, 30, 29 } },  // js@115572 total=383
  { 1808, std::chrono::year { 1808 } / 1 / 28, 5, { 29, 30, 30, 29, 30, 29, 30, 29, 30, 29, 29, 30, 29 } },  // js@115755 total=383
  { 1812, std::chrono::year { 1812 } / 2 / 13, 0, { 29, 29, 30, 29, 30, 29, 30, 29, 30, 30, 30, 29 } },  // js@116000 total=354
  { 1816, std::chrono::year { 1816 } / 1 / 29, 6, { 30, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@116244 total=384
  { 1819, std::chrono::year { 1819 } / 1 / 26, 4, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 30, 29, 30, 29 } },  // js@116427 total=384
  { 1823, std::chrono::year { 1823 } / 2 / 11, 0, { 30, 29, 30, 29, 29, 29, 30, 29, 30, 29, 30, 30 } },  // js@116672 total=354
  { 1827, std::chrono::year { 1827 } / 1 / 27, 5, { 30, 29, 30, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29 } },  // js@116916 total=384
  { 1832, std::chrono::year { 1832 } / 2 / 2, 9, { 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30 } },  // js@117222 total=384
  { 1835, std::chrono::year { 1835 } / 1 / 29, 6, { 29, 30, 30, 29, 30, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@117405 total=384
  { 1839, std::chrono::year { 1839 } / 2 / 14, 0, { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 } },  // js@117650 total=354
  { 1844, std::chrono::year { 1844 } / 2 / 18, 0, { 30, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29, 30 } },  // js@117956 total=355
  { 1847, std::chrono::year { 1847 } / 2 / 15, 0, { 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29, 30 } },  // js@118139 total=355
  { 1851, std::chrono::year { 1851 } / 2 / 1, 8, { 30, 30, 29, 30, 29, 29, 30, 29, 29, 30, 29, 30, 30 } },  // js@118383 total=384
  { 1855, std::chrono::year { 1855 } / 2 / 17, 0, { 29, 29, 30, 29, 30, 30, 29, 30, 30, 29, 30, 29 } },  // js@118628 total=354
  { 1858, std::chrono::year { 1858 } / 2 / 14, 0, { 29, 30, 29, 29, 30, 29, 29, 30, 30, 29, 30, 30 } },  // js@118811 total=354
  { 1863, std::chrono::year { 1863 } / 2 / 18, 0, { 29, 30, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30 } },  // js@119117 total=355
  { 1867, std::chrono::year { 1867 } / 2 / 5, 0, { 29, 30, 29, 29, 30, 29, 29, 30, 29, 30, 30, 30 } },  // js@119361 total=354
  { 1871, std::chrono::year { 1871 } / 2 / 19, 0, { 30, 30, 29, 30, 30, 29, 30, 29, 30, 29, 29, 30 } },  // js@119607 total=355
  { 1874, std::chrono::year { 1874 } / 2 / 17, 0, { 29, 29, 30, 29, 30, 29, 30, 29, 30, 30, 30, 29 } },  // js@119790 total=354
  { 1878, std::chrono::year { 1878 } / 2 / 2, 0, { 30, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29 } },  // js@120034 total=354
  { 1883, std::chrono::year { 1883 } / 2 / 8, 0, { 29, 29, 30, 29, 29, 30, 29, 30, 30, 30, 29, 30 } },  // js@120340 total=354
  { 1886, std::chrono::year { 1886 } / 2 / 4, 0, { 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@120523 total=354
  { 1890, std::chrono::year { 1890 } / 1 / 21, 2, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29, 30 } },  // js@120766 total=384
  { 1894, std::chrono::year { 1894 } / 2 / 6, 0, { 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30, 30 } },  // js@121011 total=354
  { 1900, std::chrono::year { 1900 } / 1 / 31, 8, { 29, 30, 29, 29, 30, 29, 30, 30, 29, 30, 30, 29, 30 } },  // js@121377 total=384
  { 1901, std::chrono::year { 1901 } / 2 / 19, 0, { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 } },  // js@121439 total=354
  { 2099, std::chrono::year { 2099 } / 1 / 21, 2, { 30, 30, 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30 } },  // js@133536 total=384
  { 2100, std::chrono::year { 2100 } / 2 / 9, 0, { 30, 30, 29, 30, 29, 30, 29, 30, 29, 29, 30, 29 } },  // js@133598 total=354
  { 2101, std::chrono::year { 2101 } / 1 / 29, 7, { 30, 30, 29, 30, 30, 29, 30, 29, 30, 29, 29, 30, 29 } },  // js@133659 total=384
  { 2103, std::chrono::year { 2103 } / 2 / 7, 0, { 29, 30, 29, 30, 29, 30, 29, 30, 30, 30, 29, 30 } },  // js@133782 total=355
  { 2105, std::chrono::year { 2105 } / 2 / 15, 0, { 29, 30, 29, 29, 30, 29, 29, 30, 30, 29, 30, 30 } },  // js@133904 total=354
  { 2108, std::chrono::year { 2108 } / 2 / 12, 0, { 30, 29, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30 } },  // js@134087 total=354
  { 2110, std::chrono::year { 2110 } / 2 / 19, 0, { 29, 30, 30, 29, 30, 30, 29, 30, 29, 29, 30, 29 } },  // js@134210 total=354
  { 2112, std::chrono::year { 2112 } / 1 / 29, 6, { 29, 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 30, 29 } },  // js@134331 total=384
  { 2115, std::chrono::year { 2115 } / 1 / 26, 4, { 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30, 30, 30 } },  // js@134514 total=384
  { 2117, std::chrono::year { 2117 } / 2 / 2, 0, { 29, 30, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30 } },  // js@134637 total=354
  { 2120, std::chrono::year { 2120 } / 1 / 30, 7, { 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29 } },  // js@134820 total=384
  { 2122, std::chrono::year { 2122 } / 2 / 7, 0, { 29, 30, 29, 30, 29, 29, 30, 29, 30, 30, 30, 29 } },  // js@134943 total=354
  { 2124, std::chrono::year { 2124 } / 2 / 15, 0, { 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 30, 29 } },  // js@135065 total=354
  { 2127, std::chrono::year { 2127 } / 2 / 11, 0, { 30, 30, 29, 30, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@135248 total=355
  { 2129, std::chrono::year { 2129 } / 2 / 19, 0, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29 } },  // js@135372 total=354
  { 2131, std::chrono::year { 2131 } / 1 / 29, 6, { 29, 30, 29, 30, 29, 29, 30, 29, 30, 30, 30, 29, 30 } },  // js@135493 total=384
  { 2133, std::chrono::year { 2133 } / 2 / 5, 0, { 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@135616 total=354
  { 2135, std::chrono::year { 2135 } / 2 / 13, 0, { 30, 29, 30, 30, 29, 29, 30, 29, 30, 29, 30, 29 } },  // js@135738 total=354
  { 2137, std::chrono::year { 2137 } / 1 / 22, 2, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29, 30 } },  // js@135859 total=384
  { 2140, std::chrono::year { 2140 } / 2 / 18, 0, { 30, 29, 30, 29, 29, 30, 29, 30, 30, 29, 30, 30 } },  // js@136044 total=355
  { 2142, std::chrono::year { 2142 } / 1 / 27, 5, { 30, 29, 30, 29, 30, 29, 29, 29, 30, 29, 30, 30, 30 } },  // js@136165 total=384
  { 2144, std::chrono::year { 2144 } / 2 / 4, 0, { 29, 30, 30, 30, 29, 29, 30, 29, 30, 29, 29, 30 } },  // js@136288 total=354
  { 2147, std::chrono::year { 2147 } / 2 / 1, 11, { 29, 29, 30, 29, 30, 30, 29, 30, 30, 29, 30, 29, 30 } },  // js@136471 total=384
  { 2149, std::chrono::year { 2149 } / 2 / 8, 0, { 30, 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30 } },  // js@136595 total=355
  { 2151, std::chrono::year { 2151 } / 2 / 16, 0, { 30, 30, 29, 30, 29, 29, 29, 30, 29, 30, 30, 29 } },  // js@136717 total=354
  { 2154, std::chrono::year { 2154 } / 2 / 12, 0, { 30, 30, 29, 30, 30, 29, 30, 29, 30, 29, 29, 30 } },  // js@136900 total=355
  { 2156, std::chrono::year { 2156 } / 1 / 23, 3, { 29, 29, 30, 29, 30, 29, 30, 30, 29, 30, 30, 29, 30 } },  // js@137021 total=384
  { 2159, std::chrono::year { 2159 } / 2 / 18, 0, { 30, 29, 30, 29, 29, 29, 30, 29, 30, 30, 29, 30 } },  // js@137205 total=354
  { 2161, std::chrono::year { 2161 } / 1 / 26, 6, { 30, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@137326 total=384
  { 2163, std::chrono::year { 2163 } / 2 / 3, 0, { 30, 30, 29, 30, 29, 30, 30, 29, 30, 29, 29, 30 } },  // js@137449 total=355
  { 2165, std::chrono::year { 2165 } / 2 / 11, 0, { 29, 30, 29, 30, 29, 30, 29, 30, 30, 30, 29, 30 } },  // js@137571 total=355
  { 2167, std::chrono::year { 2167 } / 2 / 20, 0, { 29, 30, 29, 29, 29, 30, 29, 30, 30, 29, 30, 30 } },  // js@137695 total=354
  { 2169, std::chrono::year { 2169 } / 1 / 28, 6, { 30, 29, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30, 30 } },  // js@137816 total=384
  { 2171, std::chrono::year { 2171 } / 2 / 5, 0, { 30, 29, 30, 30, 29, 30, 29, 29, 30, 29, 30, 29 } },  // js@137939 total=354
  { 2172, std::chrono::year { 2172 } / 1 / 25, 5, { 30, 29, 30, 30, 29, 30, 29, 30, 30, 29, 29, 30, 29 } },  // js@137999 total=384
  { 2175, std::chrono::year { 2175 } / 1 / 23, 3, { 29, 29, 30, 29, 29, 30, 29, 30, 30, 29, 30, 30, 30 } },  // js@138182 total=384
  { 2177, std::chrono::year { 2177 } / 1 / 30, 7, { 29, 30, 30, 29, 29, 29, 30, 29, 30, 29, 30, 30, 30 } },  // js@138305 total=384
  { 2180, std::chrono::year { 2180 } / 1 / 27, 6, { 29, 30, 30, 29, 30, 30, 29, 29, 30, 29, 30, 29, 30 } },  // js@138488 total=384
  { 2182, std::chrono::year { 2182 } / 2 / 3, 0, { 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30 } },  // js@138611 total=355
  { 2184, std::chrono::year { 2184 } / 2 / 12, 0, { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 } },  // js@138733 total=354
  { 2187, std::chrono::year { 2187 } / 2 / 8, 0, { 30, 30, 29, 30, 29, 29, 30, 29, 30, 29, 30, 29 } },  // js@138916 total=354
  { 2189, std::chrono::year { 2189 } / 2 / 15, 0, { 30, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29, 30 } },  // js@139038 total=355
  { 2191, std::chrono::year { 2191 } / 1 / 25, 5, { 30, 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29 } },  // js@139159 total=384
  { 2194, std::chrono::year { 2194 } / 1 / 22, 3, { 30, 29, 30, 29, 29, 30, 29, 30, 29, 30, 29, 30, 30 } },  // js@139342 total=384
  { 2196, std::chrono::year { 2196 } / 1 / 30, 7, { 30, 30, 29, 30, 29, 29, 30, 29, 29, 30, 29, 30, 30 } },  // js@139465 total=384
  { 2199, std::chrono::year { 2199 } / 1 / 27, 6, { 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29, 30 } },  // js@139648 total=384
};

void expect_row_matches_algo3(const Ytliu0Row& row) {
  const auto actual = algo3::calc_lunar_year(row.year);
  EXPECT_EQ(actual.date_of_first_day, row.first_day) << "year=" << row.year;
  EXPECT_EQ(actual.leap_month, row.leap_month) << "year=" << row.year;
  EXPECT_EQ(actual.month_lengths, row.month_lengths) << "year=" << row.year;
}

} // namespace

// Full sampled table (zero-tolerance civil-day fields).
TEST(LunarAlgo3Ytliu0, SampledYearsMatch) {
  ASSERT_EQ(kYtliu0Rows.size(), 115u);
  for (const auto& row : kYtliu0Rows) {
    expect_row_matches_algo3(row);
  }
}

// Endpoint / seam / #64 pins called out by name so a filter can re-run just them.
// Values come from the same table as SampledYearsMatch (no second source of truth).
TEST(LunarAlgo3Ytliu0, EndpointsAndSeams) {
  // 1603 (endpoint substitute) · 1900/1901 (lower seam) · 2099/2100 (upper seam,
  // 2099 via W-A2) · 2199 · #64 years 2133/2165/2172.
  for (const int32_t y : { 1603, 1900, 1901, 2099, 2100, 2199, 2133, 2165, 2172 }) {
    const auto it = std::find_if(
      kYtliu0Rows.begin(), kYtliu0Rows.end(),
      [y](const Ytliu0Row& r) { return r.year == y; }
    );
    ASSERT_NE(it, kYtliu0Rows.end()) << "missing named year " << y;
    expect_row_matches_algo3(*it);
  }
}

} // namespace calendar::lunar::algo3::test
