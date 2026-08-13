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

// Provenance: three oracles, applied per-path. Tolerances track what each reference supports — the
//  book's printed digits for the worked examples, and 1e-6° for the random cross-tables.
//  (1) Meeus Ch.21 worked examples — Example 21.b (ecliptic, no proper motion; book prints
//      λ=118.704°, β=1.615°, held at the book's 1e-3° digit precision) and Example 21.a inputs
//      (equatorial, proper motion zeroed to isolate precession; expected values from pymeeus at
//      PM=0, since the book's printed answer includes proper motion).
//  (2) pymeeus v0.5.11-13-g7196dff (git, 2026-08-12) — independent transcription of the same Meeus
//      formulas. 60 random (from, to) epoch pairs in the J2000 ± 2000 year window, seed 42: 30
//      equatorial + 30 ecliptic. This is the ONLY layer exercising the T₀ ≠ 0 half of (21.2)/(21.5).
//  (3) erfa.pmat76 (IAU 1976 precession matrix; erfa 2.0.1.5.dev2+gd4d4fd5) — the same ζ/z/θ angles
//      assembled as a rotation matrix, so it cross-checks the (21.4) transform step and the
//      independent transcription. It precesses from J2000 only: 12 equatorial rows, T₀ = 0; the
//      ecliptic path has no erfa layer.

#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <vector>

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "coord_transform.hpp"
#include "earth/precession.hpp"


namespace astro::earth::precession::test {

using namespace astro::earth::precession;
using astro::toolbox::AngleDeg;
using astro::julian_day::J2000;

// Smallest signed difference a−b in degrees, aware of the [0°, 360°) wrap.
[[nodiscard]] inline auto angdiff(const double a, const double b) -> double {
  double d = std::fmod(a - b + 180.0, 360.0);
  if (d < 0.0) { d += 360.0; }
  return d - 180.0;
}


// ---- Meeus (21.2): the angle polynomials at a clean point (T₀ = 0, t = 1 century) -------------

TEST(Precession, EquatorialAnglesAtOneCentury) {
  // J2000 → J2000 + 1 Julian century: T₀ = 0, t = 1, so only the t-linear/t-quadratic terms remain.
  const auto [ζ, z, θ] = equatorial_angles(J2000, J2000 + 36525.0);
  // Expected (arcsec): ζ = 2306.2181 + 0.30188 + 0.017998, z = 2306.2181 + 1.09468 + 0.018203,
  // θ = 2004.3109 − 0.42665 − 0.041833.
  EXPECT_NEAR(ζ.deg() * 3600.0, 2306.537978, 1e-6);
  EXPECT_NEAR(z.deg()  * 3600.0, 2307.330983, 1e-6);
  EXPECT_NEAR(θ.deg() * 3600.0, 2003.842417, 1e-6);
}

TEST(Precession, EclipticAnglesAtOneCentury) {
  const auto [η, Π, p] = ecliptic_angles(J2000, J2000 + 36525.0);
  // η = 47.0029 − 0.03302 + 0.00006 arcsec; p = 5029.0966 + 1.11113 − 0.000006 arcsec;
  // Π = 174.876384° + (−869.8089 + 0.03536)/3600 deg.
  EXPECT_NEAR(η.deg() * 3600.0, 46.96994, 1e-6);
  EXPECT_NEAR(Π.deg(),           174.634780239, 1e-9);
  EXPECT_NEAR(p.deg() * 3600.0, 5030.207724, 1e-6);
}


// ---- Meeus worked examples (the absolute-correctness anchor) -----------------------------------

TEST(Precession, MeeusExample21bEcliptic) {
  // Example 21.b: J2000 → epoch −214/6/30 (JDE 1643074.5), λ₀ = 149.48194°, β₀ = 1.76549°.
  // Book prints λ = 118.704°, β = 1.615°.
  const auto result = ecliptic(AngleDeg { 149.48194 }, AngleDeg { 1.76549 }, J2000, 1643074.5);
  // Book values at printed precision (3 decimals).
  EXPECT_NEAR(angdiff(result.λ.deg(), 118.704), 0.0, 1e-3);
  EXPECT_NEAR(result.β.deg() - 1.615, 0.0, 1e-3);
  // pymeeus high-precision values for the same inputs.
  EXPECT_NEAR(angdiff(result.λ.deg(), 118.704168), 0.0, 1e-6);
  EXPECT_NEAR(result.β.deg() - 1.615332, 0.0, 1e-6);
}

TEST(Precession, MeeusExample21aEquatorialNoProperMotion) {
  // Example 21.a inputs, proper motion zeroed to isolate precession. J2000 → 2028/11/13.19
  // (JDE 2462088.69), α₀ = 2h44m11.986s = 41.0499417°, δ₀ = 49°13'42.48" = 49.2284667°.
  // Reference: pymeeus with proper motion = 0 → α = 41.5430861°, δ = 49.3492074°.
  const auto result = equatorial(
    AngleDeg { 41.0499416667 }, AngleDeg { 49.2284666667 }, J2000, 2462088.69);
  EXPECT_NEAR(angdiff(result.α.deg(), 41.5430860713), 0.0, 1e-6);
  EXPECT_NEAR(result.δ.deg() - 49.3492074123, 0.0, 1e-6);
}


// ---- pymeeus cross-validation (independent transcription, both coordinate systems) -------------

TEST(Precession, PymeeusEquatorialCross) {
  // Columns: jde_from, jde_to, α₀ (deg), δ₀ (deg), α_pymeeus (deg), δ_pymeeus (deg). Seed 42.
  const std::vector<std::array<double, 6>> data {
    { 2655247.552547, 1757585.713380, 99.0105546129, -49.2684886095, 83.2849581510, -49.0090898136 },
    { 2797029.443894, 2709702.951125, 321.1846443737, -73.5248877920, 315.0126001295, -74.5123958885 },
    { 2337472.778560, 1764578.737599, 78.7096709293, 0.9532412824, 58.7982916667, -2.2188997516 },
    { 1759814.051708, 2011546.807653, 233.9583976006, 7.9995835474, 242.3076949319, 5.9615821561 },
    { 2043108.748801, 2581962.164143, 291.3949644040, -87.8432207773, 358.1479175746, -80.5237649586 },
    { 2898346.926928, 2741026.656078, 122.4901859465, -61.3246490335, 120.5638893623, -60.0759058990 },
    { 3119533.298494, 2212809.630410, 33.3885036169, -71.7844849236, 38.3895712114, -83.4379445583 },
    { 2959234.269234, 2603088.731827, 290.5661783788, 40.8922580315, 282.3627536805, 39.3614105617 },
    { 2504474.241615, 3142767.131174, 136.2723757950, 9.2632323666, 159.3217386940, 1.1161457428 },
    { 2932805.214474, 2624702.358204, 310.2144841119, 13.7686818557, 300.1835491132, 11.0753911344 },
    { 2750424.452710, 1787994.424521, 82.0433792346, -37.4889424788, 59.7206297484, -42.2996098491 },
    { 1837621.078285, 2061152.484973, 36.3605145875, -39.5206986464, 42.4280123178, -36.8734737467 },
    { 2649779.973070, 2254064.813475, 133.2651481621, -51.7077485227, 124.9078561587, -47.9085129757 },
    { 2111099.598014, 3089497.352648, 233.2927386888, 19.4253190087, 263.7712406635, 14.0103392019 },
    { 1971078.565017, 2786299.251805, 58.8248977543, -21.4569313671, 83.3715169025, -17.4422568580 },
    { 3166738.615280, 2656084.649147, 200.5019077589, 32.8613366762, 183.6228973053, 40.4029717432 },
    { 2952451.655397, 2854780.870769, 82.4573059071, -83.2861565851, 91.6662458288, -83.3625471090 },
    { 2181921.903214, 2112214.419801, 75.9538236911, 78.8379291516, 68.4500847437, 78.5116848125 },
    { 3001418.102277, 2180789.383847, 235.9579195062, -18.5775216112, 205.0051238245, -9.1541222534 },
    { 3057199.028611, 2391427.556630, 95.3568599393, -45.1003036305, 81.9960266281, -45.3403377217 },
    { 2541203.844012, 2104910.490052, 210.4509564805, 70.8124732812, 205.9532964304, 76.6930896555 },
    { 2304569.138010, 2041472.629129, 359.1135383382, 1.6956802744, 349.9214183493, -2.3049656584 },
    { 1853863.651186, 1789882.024496, 39.4736869262, 22.6853954232, 37.0033798775, 21.9138125438 },
    { 2878272.951334, 2337820.711494, 22.8699742147, -21.0717670018, 4.7429036496, -29.0226167154 },
    { 3176378.336531, 2494081.058190, 349.5882159409, 64.2187869977, 331.2791826732, 54.5211387656 },
    { 1737818.773058, 2774019.578085, 245.4157328496, 6.5807188128, 280.3526557268, 4.6165948336 },
    { 2110876.602521, 2657490.187725, 40.1587824945, -11.6117853809, 58.3593404576, -6.1867196118 },
    { 2383935.334947, 3114570.070108, 315.3070585361, -42.1167489663, 345.3198133093, -32.5464140042 },
    { 2452401.311166, 1982055.397455, 328.5460221641, 65.9523054309, 320.3021835548, 60.1090476805 },
    { 2157072.840306, 2654550.211999, 219.2292761177, -61.7946101982, 249.3227012755, -66.2408302424 },
  };
  for (const auto& [jf, jt, a0, d0, ra_e, dec_e] : data) {
    const auto result = equatorial(AngleDeg { a0 }, AngleDeg { d0 }, jf, jt);
    ASSERT_NEAR(angdiff(result.α.deg(), ra_e), 0.0, 1e-6) << "row {α₀=" << a0 << ", δ₀=" << d0 << "}";
    ASSERT_NEAR(result.δ.deg() - dec_e, 0.0, 1e-6)        << "row {α₀=" << a0 << ", δ₀=" << d0 << "}";
  }
}

TEST(Precession, PymeeusEclipticCross) {
  // Columns: jde_from, jde_to, λ₀ (deg), β₀ (deg), λ_pymeeus (deg), β_pymeeus (deg). Seed 42.
  const std::vector<std::array<double, 6>> data {
    { 2835073.278910, 2509077.763005, 280.3055323070, 5.4029536507, 267.8046254876, 5.5182993662 },
    { 1721880.540243, 2194636.999284, 7.0116272589, 76.3795536951, 24.3866184306, 76.4738086480 },
    { 3004857.663500, 2936108.338397, 110.7050851450, -78.6893203640, 108.0832248578, -78.7129833342 },
    { 3003817.024437, 3104538.139580, 30.8352427444, -2.4936975296, 34.7170124162, -2.4800337241 },
    { 1822164.489482, 2832284.763441, 275.7003945505, -66.1463193190, 314.7301424851, -66.4489769855 },
    { 2415432.554402, 2524308.050096, 95.4203864184, 66.2930813132, 99.6041670653, 66.3310974459 },
    { 2339249.530633, 2030482.178151, 194.1465919606, 40.9277302980, 182.2843087722, 40.9658747885 },
    { 2014926.703612, 2176462.501591, 358.2537683979, 26.6782942598, 4.3770152901, 26.6937801180 },
    { 2361109.222599, 2477223.303753, 43.5615105126, -49.0038740084, 48.0303831181, -48.9712477013 },
    { 2214988.006297, 2580564.037666, 82.8413037348, -49.8013055676, 96.7974941883, -49.6711611913 },
    { 1824765.898659, 2643086.420572, 82.4190421720, 72.1647623151, 113.9308272829, 72.4440603579 },
    { 2976972.319771, 1824567.588187, 85.6816683728, 30.0780445367, 41.7137843785, 29.6909859492 },
    { 2034044.975568, 1914352.610987, 336.7851266090, 12.6456706119, 332.2433901477, 12.6520871156 },
    { 2411617.369442, 2867373.978889, 290.6989191960, -55.1070352436, 308.2857483143, -55.2472649588 },
    { 1862660.919588, 2350810.777496, 152.4883042872, -5.8696090895, 171.0625595055, -5.8556899187 },
    { 2786224.816061, 2704830.603596, 354.2994760917, -71.4816189350, 351.0905712563, -71.4768436761 },
    { 2309274.693151, 2216766.106482, 310.2021130870, -44.7391725622, 306.6473820319, -44.7169900965 },
    { 1998940.215232, 2376469.393384, 151.8773903404, -39.4189642493, 166.1600571940, -39.3969733026 },
    { 2086012.220356, 3069936.040542, 159.5270682192, 64.3201406476, 197.9436927253, 64.3164461622 },
    { 2525070.281489, 1794954.549436, 359.7416886286, 59.8129101442, 332.3690611272, 59.8202208624 },
    { 3136748.531893, 3074467.162175, 305.5304643891, -59.3966223125, 303.1151380605, -59.3765652241 },
    { 2430566.684283, 2033329.804130, 144.3745053178, -78.5628988050, 129.8222180302, -78.6394458365 },
    { 2274724.726825, 3160581.220762, 95.4731009420, 50.5645671468, 129.6002594564, 50.8571903456 },
    { 2385812.224682, 2339058.937032, 344.6343507095, 88.1852387297, 343.3717789139, 88.1877597307 },
    { 2533022.520496, 2770639.490208, 55.7268570987, -36.1860070620, 64.8674327226, -36.1126993268 },
    { 3136329.382220, 2567227.404883, 195.1902724947, 44.1396497475, 173.1239002225, 44.1369138373 },
    { 1804563.463718, 2574528.465505, 181.0261378510, 62.7841407846, 210.8594729620, 62.6555450245 },
    { 1951054.215520, 3124742.977684, 28.8401274866, -55.9231569454, 74.1346673458, -55.5841218530 },
    { 2590391.290523, 2707530.540816, 84.6734022003, -67.6601827174, 89.1694374686, -67.6186661307 },
    { 3021754.765943, 2080765.623119, 214.0268952720, 21.2499088391, 177.8696889301, 21.3560797301 },
  };
  for (const auto& [jf, jt, l0, b0, lon_e, lat_e] : data) {
    const auto result = ecliptic(AngleDeg { l0 }, AngleDeg { b0 }, jf, jt);
    ASSERT_NEAR(angdiff(result.λ.deg(), lon_e), 0.0, 1e-6) << "row {λ₀=" << l0 << ", β₀=" << b0 << "}";
    ASSERT_NEAR(result.β.deg() - lat_e, 0.0, 1e-6)        << "row {λ₀=" << l0 << ", β₀=" << b0 << "}";
  }
}


// ---- erfa.pmat76 (IAU 1976 matrix, from J2000) — independent implementation -------------------

TEST(Precession, ErfaPmat76Cross) {
  // Columns: jde_to (from = J2000), α₀ (deg), δ₀ (deg), α_erfa (deg), δ_erfa (deg). Seed 42.
  const std::vector<std::array<double, 5>> data {
    { 2333532.601306, 210.1220241448, 4.0553233647, 206.0579641786, 5.6430956107 },
    { 3086650.842553, 73.5333117925, 38.4821405405, 103.6547378542, 38.7239953518 },
    { 2069765.176772, 142.4829048449, 30.5608596869, 126.4629449804, 34.6396229525 },
    { 2159340.733586, 113.8237906579, 44.8318796498, 99.2485139591, 46.1010445318 },
    { 1827030.490275, 164.9827881427, 88.7248904721, 352.9496004807, 81.6317846030 },
    { 3176341.910316, 26.3738595959, -51.0585324165, 45.2562455322, -42.1815325827 },
    { 2108502.805950, 335.9733760777, 67.7938229162, 329.2452770374, 63.1450938223 },
    { 3005658.824270, 133.0297519460, -60.9210638404, 140.8119228734, -67.0758212155 },
    { 2939146.378729, 253.2743730315, 19.8786422992, 267.9208196424, 18.6679407999 },
    { 3163392.505966, 235.4314743759, -87.6074869269, 359.8470144025, -79.9326509634 },
    { 2914834.141404, 107.7763507920, 29.0831912640, 127.3490935375, 25.8327256835 },
    { 3092821.735737, 48.3448011816, -68.4536966654, 54.4159351426, -62.3375036551 },
  };
  for (const auto& [jt, a0, d0, ra_e, dec_e] : data) {
    const auto result = equatorial(AngleDeg { a0 }, AngleDeg { d0 }, J2000, jt);
    ASSERT_NEAR(angdiff(result.α.deg(), ra_e), 0.0, 1e-6) << "row {α₀=" << a0 << ", δ₀=" << d0 << "}";
    ASSERT_NEAR(result.δ.deg() - dec_e, 0.0, 1e-6)        << "row {α₀=" << a0 << ", δ₀=" << d0 << "}";
  }
}


// ---- Pole clamp (asin(C) roundoff guard) ------------------------------------------------------

TEST(Precession, EquatorialClampGuardsPoleNan) {
  // Feed the input that precesses to the north celestial pole: α₀ = -ζ, δ₀ = 90° - θ. Removing the
  // clamp in equatorial() fails this test (verified by mutation).
  constexpr double kCentury = 36525.0;
  for (const double dt : { 0.014, 1.0, 5.0 }) {
    const auto jde_to = J2000 + (dt * kCentury);
    const auto ang = equatorial_angles(J2000, jde_to);
    const auto r = equatorial(AngleDeg { -ang.ζ.deg() }, AngleDeg { 90.0 - ang.θ.deg() }, J2000, jde_to);
    ASSERT_TRUE(std::isfinite(r.δ.deg())) << "dt=" << dt;
    ASSERT_LE(r.δ.deg(), 90.0);
  }
}


// ---- Property tests ---------------------------------------------------------------------------

TEST(Precession, LongitudeDriftsMonotonically) {
  // For an ecliptic-plane body (β₀ = 0), λ increases monotonically under precession (~50"/year).
  constexpr double kCentury = 36525.0;
  double prev = -1.0;
  for (int k = 0; k <= 5; ++k) {
    const double lon = ecliptic(AngleDeg { 0.0 }, AngleDeg { 0.0 }, J2000, J2000 + (k * kCentury)).λ.deg();
    if (k > 0) {
      ASSERT_GT(lon, prev);
      // Per-century drift ≈ 1.397° ≈ 5029"/cy ≈ 50.3"/yr.
      ASSERT_NEAR(lon - prev, 1.40, 0.01);
    }
    prev = lon;
  }
}

TEST(Precession, DeclinationTurnsUnderPrecession) {
  // Counterpart to LongitudeDriftsMonotonically: declination does NOT drift monotonically. For a
  // star at (α₀ = 90°, δ₀ = 40°), δ peaks at J2000 and falls off symmetrically on both sides — a
  // turn, not a monotone drift.
  constexpr double kCentury = 36525.0;
  const auto dec_at = [](const double k) {
    return equatorial(AngleDeg { 90.0 }, AngleDeg { 40.0 }, J2000, J2000 + (k * kCentury)).δ.deg();
  };
  EXPECT_GT(dec_at(0.0), dec_at(2.0));
  EXPECT_GT(dec_at(0.0), dec_at(-2.0));
}

} // namespace astro::earth::precession::test
