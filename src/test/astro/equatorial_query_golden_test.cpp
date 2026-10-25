/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include <gtest/gtest.h>

#include "ephemeris.hpp"

// Retained material boundaries: V07 identifies the JPL Horizons Q2 rows; V20 identifies the
// SOFA 2023-10-11-derived projection and node rows. These references remain under their
// source terms and outside the project MIT grant.

namespace astro::ephemeris::test {

namespace {

/** @brief TT input and expected apparent equatorial direction, in degrees. */
struct EquatorialRow {
  Target target;
  double jde_tt;
  double α_deg;
  double δ_deg;
};

} // namespace

TEST(EquatorialQueryGolden, PhysicalTargetsVsHorizons) {
  // JPL Horizons Q2, collected 2026-10-24: observer 500@399, TIME_TYPE=TT, TLIST_TYPE=JD,
  // AIRLESS, ANG_FORMAT=DEG, EXTRA_PREC=YES, REF_SYSTEM=ICRF. Q2 is apparent true equator/equinox
  // of date (EOP-corrected IAU76/80), not fixed-ICRF astrometric RA/Dec.
  // https://ssd.jpl.nasa.gov/horizons/manual.html#obsquan
  // Commands: 10, 301, 199, 299, 499, 599, 699, 799, 899, 999 (physical centers).
  // Eight random.Random(20261024).uniform(2411072.272126, 2469811.185372) draws rounded to six
  // JD decimals, plus 2451545.0, 2460000.5, 2460687.5 and 2461120.0. Inputs are the requested TT
  // tokens, not Horizons' slightly different echoes. Actual sample coverage is 1892-2026.
  // Per-column ceilings are regression thresholds for these 12 rows per target, not global accuracy
  // bounds. RA has no cos(δ) factor.
  const std::unordered_map<Target, std::array<double, 2>> tolerances_arcsec {
    { Target::SUN,     { 0.4, 0.2  } },
    { Target::MOON,    { 6.0, 4.0  } },
    { Target::MERCURY, { 0.2, 0.1  } },
    { Target::VENUS,   { 0.2, 0.1  } },
    { Target::MARS,    { 0.3, 0.1  } },
    { Target::JUPITER, { 0.5, 0.25 } },
    { Target::SATURN,  { 0.4, 0.2  } },
    { Target::URANUS,  { 1.5, 0.5  } },
    { Target::NEPTUNE, { 2.0, 1.0  } },
    { Target::PLUTO,   { 1.0, 0.5  } },
  };

  // NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
  // Retained data: equatorial-horizons.
  constexpr std::array dataset {
    // Target                         Requested JDE(TT)  RA (deg)       Dec (deg)
    EquatorialRow { Target::SUN,         2412175.329074, 357.894406865,  -0.913280380 },
    EquatorialRow { Target::SUN,         2429178.442349, 191.967542581,  -5.138508845 },
    EquatorialRow { Target::SUN,         2441975.740939, 204.763947784, -10.295089004 },
    EquatorialRow { Target::SUN,         2443886.721958, 294.212937771, -21.574573378 },
    EquatorialRow { Target::SUN,         2445654.410718, 230.562739920, -18.515016651 },
    EquatorialRow { Target::SUN,         2451545.000000, 281.277554826, -23.032489039 },
    EquatorialRow { Target::SUN,         2453720.394735, 263.481844985, -23.305381685 },
    EquatorialRow { Target::SUN,         2454673.347801, 125.538669894,  19.433313651 },
    EquatorialRow { Target::SUN,         2456183.746539, 171.593555902,   3.626009170 },
    EquatorialRow { Target::SUN,         2460000.500000, 337.880366218,  -9.271460620 },
    EquatorialRow { Target::SUN,         2460687.500000, 293.792278163, -21.637629134 },
    EquatorialRow { Target::SUN,         2461120.000000, 359.894113398,  -0.045804649 },
    EquatorialRow { Target::MOON,        2412175.329074, 221.903630037, -15.814434439 },
    EquatorialRow { Target::MOON,        2429178.442349, 343.094812841,  -2.121055519 },
    EquatorialRow { Target::MOON,        2441975.740939, 135.032283918,  13.445782591 },
    EquatorialRow { Target::MOON,        2443886.721958, 112.622611778,  17.479521834 },
    EquatorialRow { Target::MOON,        2445654.410718,   0.749471376,  -5.248288666 },
    EquatorialRow { Target::MOON,        2451545.000000, 222.443585389, -10.897906428 },
    EquatorialRow { Target::MOON,        2453720.394735,  86.089305433,  28.253670212 },
    EquatorialRow { Target::MOON,        2454673.347801,  30.070024021,  17.687354365 },
    EquatorialRow { Target::MOON,        2456183.746539, 135.558439568,  11.768549515 },
    EquatorialRow { Target::MOON,        2460000.500000,  36.174129740,  14.614950535 },
    EquatorialRow { Target::MOON,        2460687.500000,  87.097384497,  28.444066422 },
    EquatorialRow { Target::MOON,        2461120.000000,  16.094927527,  10.498816684 },
    EquatorialRow { Target::MERCURY,     2412175.329074,   8.131327907,   3.496438147 },
    EquatorialRow { Target::MERCURY,     2429178.442349, 190.035316043,  -2.815460756 },
    EquatorialRow { Target::MERCURY,     2441975.740939, 227.877171030, -20.911654965 },
    EquatorialRow { Target::MERCURY,     2443886.721958, 276.814829607, -23.791650146 },
    EquatorialRow { Target::MERCURY,     2445654.410718, 240.014642960, -21.927838949 },
    EquatorialRow { Target::MERCURY,     2451545.000000, 272.073366975, -24.418845941 },
    EquatorialRow { Target::MERCURY,     2453720.394735, 241.710312808, -18.960224911 },
    EquatorialRow { Target::MERCURY,     2454673.347801, 121.057348058,  21.773563737 },
    EquatorialRow { Target::MERCURY,     2456183.746539, 174.398686253,   4.025681826 },
    EquatorialRow { Target::MERCURY,     2460000.500000, 323.225688334, -16.637619739 },
    EquatorialRow { Target::MERCURY,     2460687.500000, 275.614496971, -23.683765143 },
    EquatorialRow { Target::MERCURY,     2461120.000000, 339.724491407,  -7.397523133 },
    EquatorialRow { Target::VENUS,       2412175.329074,  36.119892174,  15.545622583 },
    EquatorialRow { Target::VENUS,       2429178.442349, 232.159305145, -24.230359726 },
    EquatorialRow { Target::VENUS,       2441975.740939, 250.347107770, -25.026563346 },
    EquatorialRow { Target::VENUS,       2443886.721958, 244.451518338, -17.573756391 },
    EquatorialRow { Target::VENUS,       2445654.410718, 186.822123825,  -1.370722966 },
    EquatorialRow { Target::VENUS,       2451545.000000, 239.891836699, -18.448719557 },
    EquatorialRow { Target::VENUS,       2453720.394735, 302.525265519, -21.282219565 },
    EquatorialRow { Target::VENUS,       2454673.347801, 138.969882152,  17.381183960 },
    EquatorialRow { Target::VENUS,       2456183.746539, 129.363957687,  17.439223374 },
    EquatorialRow { Target::VENUS,       2460000.500000,   5.600246343,   1.505550872 },
    EquatorialRow { Target::VENUS,       2460687.500000, 340.954618512,  -8.586555888 },
    EquatorialRow { Target::VENUS,       2461120.000000,  16.415712303,   6.038367467 },
    EquatorialRow { Target::MARS,        2412175.329074, 272.684271684, -23.574523752 },
    EquatorialRow { Target::MARS,        2429178.442349, 169.826107565,   5.664233059 },
    EquatorialRow { Target::MARS,        2441975.740939,  31.749064542,  10.579609959 },
    EquatorialRow { Target::MARS,        2443886.721958, 296.245704156, -22.260448620 },
    EquatorialRow { Target::MARS,        2445654.410718, 179.225857919,   1.961329769 },
    EquatorialRow { Target::MARS,        2451545.000000, 330.516247655, -13.182689444 },
    EquatorialRow { Target::MARS,        2453720.394735,  35.660620747,  15.486575686 },
    EquatorialRow { Target::MARS,        2454673.347801, 166.183194769,   6.863691133 },
    EquatorialRow { Target::MARS,        2456183.746539, 220.851050287, -16.519027518 },
    EquatorialRow { Target::MARS,        2460000.500000,  76.222818878,  25.299402525 },
    EquatorialRow { Target::MARS,        2460687.500000, 120.861081845,  24.727191411 },
    EquatorialRow { Target::MARS,        2461120.000000, 345.768683481,  -7.249953481 },
    EquatorialRow { Target::JUPITER,     2412175.329074,   0.616132171,  -0.908467913 },
    EquatorialRow { Target::JUPITER,     2429178.442349, 325.398541031, -15.096210560 },
    EquatorialRow { Target::JUPITER,     2441975.740939, 305.534014779, -20.221910176 },
    EquatorialRow { Target::JUPITER,     2443886.721958, 128.071401580,  19.486234822 },
    EquatorialRow { Target::JUPITER,     2445654.410718, 254.361437385, -22.267297680 },
    EquatorialRow { Target::JUPITER,     2451545.000000,  23.867809514,   8.594247379 },
    EquatorialRow { Target::JUPITER,     2453720.394735, 218.425622883, -13.955485095 },
    EquatorialRow { Target::JUPITER,     2454673.347801, 286.751647802, -22.779379653 },
    EquatorialRow { Target::JUPITER,     2456183.746539,  74.502630527,  21.861738688 },
    EquatorialRow { Target::JUPITER,     2460000.500000,  10.532958095,   3.316911278 },
    EquatorialRow { Target::JUPITER,     2460687.500000,  70.793041888,  21.695582057 },
    EquatorialRow { Target::JUPITER,     2461120.000000, 106.571250466,  22.929299119 },
    EquatorialRow { Target::SATURN,      2412175.329074, 177.975349631,   3.607576149 },
    EquatorialRow { Target::SATURN,      2429178.442349,  14.622335109,   3.266299926 },
    EquatorialRow { Target::SATURN,      2441975.740939,  95.127695423,  22.271588106 },
    EquatorialRow { Target::SATURN,      2443886.721958, 165.624349772,   8.205495302 },
    EquatorialRow { Target::SATURN,      2445654.410718, 217.438825480, -12.452933321 },
    EquatorialRow { Target::SATURN,      2451545.000000,  38.765401696,  12.614764759 },
    EquatorialRow { Target::SATURN,      2453720.394735, 133.422626645,  18.090620357 },
    EquatorialRow { Target::SATURN,      2454673.347801, 159.383917032,  10.474380326 },
    EquatorialRow { Target::SATURN,      2456183.746539, 206.328321266,  -8.439713514 },
    EquatorialRow { Target::SATURN,      2460000.500000, 331.337112228, -13.101126983 },
    EquatorialRow { Target::SATURN,      2460687.500000, 347.352580433,  -7.543724439 },
    EquatorialRow { Target::SATURN,      2461120.000000,   4.617323147,  -0.314577650 },
    EquatorialRow { Target::URANUS,      2412175.329074, 213.356150189, -12.868635077 },
    EquatorialRow { Target::URANUS,      2429178.442349,  44.710893238,  16.568298545 },
    EquatorialRow { Target::URANUS,      2441975.740939, 202.091613662,  -8.654463143 },
    EquatorialRow { Target::URANUS,      2443886.721958, 227.851778163, -17.470993762 },
    EquatorialRow { Target::URANUS,      2445654.410718, 246.634302632, -21.642396620 },
    EquatorialRow { Target::URANUS,      2451545.000000, 317.474773557, -17.020341543 },
    EquatorialRow { Target::URANUS,      2453720.394735, 339.216089175,  -9.565714781 },
    EquatorialRow { Target::URANUS,      2454673.347801, 353.273372405,  -3.772244369 },
    EquatorialRow { Target::URANUS,      2456183.746539,   6.897044879,   2.162817441 },
    EquatorialRow { Target::URANUS,      2460000.500000,  43.047634385,  16.136821163 },
    EquatorialRow { Target::URANUS,      2460687.500000,  51.090231313,  18.384557986 },
    EquatorialRow { Target::URANUS,      2461120.000000,  56.095149333,  19.611324603 },
    EquatorialRow { Target::NEPTUNE,     2412175.329074,  64.947503482,  19.877387458 },
    EquatorialRow { Target::NEPTUNE,     2429178.442349, 172.820563050,   4.305442390 },
    EquatorialRow { Target::NEPTUNE,     2441975.740939, 244.126889091, -19.728521421 },
    EquatorialRow { Target::NEPTUNE,     2443886.721958, 258.431194547, -21.624958216 },
    EquatorialRow { Target::NEPTUNE,     2445654.410718, 267.482735241, -22.247793199 },
    EquatorialRow { Target::NEPTUNE,     2451545.000000, 305.432804763, -19.213246907 },
    EquatorialRow { Target::NEPTUNE,     2453720.394735, 318.013749296, -16.337066784 },
    EquatorialRow { Target::NEPTUNE,     2454673.347801, 325.843150185, -14.045724935 },
    EquatorialRow { Target::NEPTUNE,     2456183.746539, 333.475548827, -11.622683814 },
    EquatorialRow { Target::NEPTUNE,     2460000.500000, 355.328056020,  -3.295382113 },
    EquatorialRow { Target::NEPTUNE,     2460687.500000, 358.198184588,  -2.175040738 },
    EquatorialRow { Target::NEPTUNE,     2461120.000000,   2.142487245,  -0.494808452 },
    EquatorialRow { Target::PLUTO,       2412175.329074,  67.080732403,  10.345991044 },
    EquatorialRow { Target::PLUTO,       2429178.442349, 124.356361795,  22.973271393 },
    EquatorialRow { Target::PLUTO,       2441975.740939, 191.179761237,  12.700782874 },
    EquatorialRow { Target::PLUTO,       2443886.721958, 204.208753434,   8.375935770 },
    EquatorialRow { Target::PLUTO,       2445654.410718, 213.978051295,   3.800993447 },
    EquatorialRow { Target::PLUTO,       2451545.000000, 251.419143774, -11.394271924 },
    EquatorialRow { Target::PLUTO,       2453720.394735, 264.115571536, -15.842923103 },
    EquatorialRow { Target::PLUTO,       2454673.347801, 268.949271956, -17.134777571 },
    EquatorialRow { Target::PLUTO,       2456183.746539, 277.376680799, -19.614767371 },
    EquatorialRow { Target::PLUTO,       2460000.500000, 302.069583254, -22.566043820 },
    EquatorialRow { Target::PLUTO,       2460687.500000, 304.432357972, -23.047170882 },
    EquatorialRow { Target::PLUTO,       2461120.000000, 308.376873585, -22.809039944 },
  };
  // NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

  for (const auto& [target, jde_tt, α, δ] : dataset) {
    SCOPED_TRACE(::testing::Message() << "target=" << static_cast<uint32_t>(target) << " jde_tt=" << jde_tt);
    const auto result = equatorial_position(target, jde_tt);
    const auto& [α_tol, δ_tol] = tolerances_arcsec.at(target);
    ASSERT_NEAR(std::remainder(result.right_ascension.deg() - α, 360.0), 0.0, α_tol / 3600.0);
    ASSERT_NEAR(result.declination.deg(), δ, δ_tol / 3600.0);
  }
}

TEST(EquatorialQueryGolden, FixedProviderInputsVsIndependentProjection) {
  // Generated 2026-10-24 at the same TT inputs as the Horizons set above. Ecliptic inputs are frozen
  // from commit 52fda3c735f6a96dbf9beaddb8034f522500b80f, not an independent physical ephemeris.
  // SOFA issue 2023-10-11 iauObl80 + iauNut80 supplies true obliquity, with the two-part TT date
  // (2400000.5, jde_tt - 2400000.5). Python rotates the unit Cartesian vector about x, then uses
  // atan2(y, x) for RA and atan2(z, hypot(x, y)) for Dec. https://www.iausofa.org/2023-10-11c
  // The 1e-9 deg ceiling checks projection/obliquity conformance, separately from physical accuracy.
  constexpr double TOLERANCE_DEG = 1e-9;

  // Retained data: equatorial-projection.
  // Frozen apparent ecliptic longitude/latitude in degrees, in the reference table's row order.
  constexpr std::array provider_inputs {
    std::array {  357.70494373802023,   1.720078391707955e-05 },
    std::array {   193.0092490364145,  0.00016324348359164392 },
    std::array {    206.693520392924, -0.00020397487720110507 },
    std::array {  292.42012776384058, -6.9898831289906012e-05 },
    std::array {    232.961353255424, -9.9481758725480872e-05 },
    std::array {  280.36816129477182,  0.00023986985453378364 },
    std::array {  264.01570914569498,  0.00013266435226465606 },
    std::array {  123.23937884856207,  0.00019842943020817097 },
    std::array {  170.85006082085565, -0.00022068413510388982 },
    std::array {  336.10615043421541, -3.5158259307909527e-05 },
    std::array {  292.02400980944572,  0.00013217681290542336 },
    std::array {  359.88465717834407,  9.7971703755848425e-05 },
    std::array {  224.26609181858748,     0.33082838486978638 },
    std::array {  343.60293391262013,       4.683997223661474 },
    std::array {  133.57796837376861,     -3.4460641095699729 },
    std::array {  111.58792285226393,     -4.2804737815811764 },
    std::array {  358.59345498366002,     -5.1120479007127821 },
    std::array {  223.31484342022236,      5.1712800717147029 },
    std::array {  86.543456907913708,      4.8598569517933639 },
    std::array {  34.128809368381191,      5.0982154828332193 },
    std::array {  134.54912272159163,     -4.9045126396765353 },
    std::array {  38.635997981017063,     0.24775269982229198 },
    std::array {  87.436979909293925,      5.0318998448860253 },
    std::array {  18.851196359992645,      3.3684930984014692 },
    std::array {  8.8465156002929533,   -0.014111778190225605 },
    std::array {  190.32730444706891,      1.3858368091853868 },
    std::array {  231.14245214215532,      -2.970670032571157 },
    std::array {  276.23352290333099,    -0.49965052845179098 },
    std::array {  242.37099476580542,     -1.3162782801010571 },
    std::array {  271.88813108285046,    -0.99474529273519852 },
    std::array {  243.35487306819391,      1.9018210892796326 },
    std::array {  118.63494066544942,      1.3679316491884801 },
    std::array {  173.26409041176797,      1.4718910358041128 },
    std::array {  320.16785377159533,     -1.9791183536033332 },
    std::array {    275.140414188813,    -0.34547167602879031 },
    std::array {   338.4957026515238,      1.0636119371922255 },
    std::array {  38.886843009957843,      1.1353582446265771 },
    std::array {  235.82637828850542,     -5.1629900658907264 },
    std::array {  252.23547131103368,     -2.7877226640814206 },
    std::array {  245.66813052245519,      3.7335059636265164 },
    std::array {  186.80589532366568,      1.4494748964602462 },
    std::array {    241.564894429464,      2.0663762978545499 },
    std::array {  300.07343595304064,     -1.1730753262384208 },
    std::array {  136.06620705862326,      1.4249738395330867 },
    std::array {  127.24281964441376,     -1.0540940744467648 },
    std::array {  5.7372237288387495,    -0.84212103524563897 },
    std::array {  339.17980693812484,    -0.49515411313675595 },
    std::array {  17.441119010116498,     -0.8750155714448673 },
    std::array {   272.4600874546112,    -0.14233442875234376 },
    std::array {  168.42943884523834,      1.1810996354121446 },
    std::array {  33.228833435049346,     -2.1396673410315099 },
    std::array {  294.16240644497071,    -0.99572271875611384 },
    std::array {  178.50941144032961,      1.4914808768712813 },
    std::array {  327.96272701479785,     -1.0677792264837249 },
    std::array {  38.448610973634764,      1.2310854483036939 },
    std::array {   164.6265259000613,     0.87820056200340246 },
    std::array {  223.51124202314395,     -0.6566051075623387 },
    std::array {   77.55498719155834,      2.4542001842362056 },
    std::array {   117.8523060357536,      4.2233984978012762 },
    std::array {  344.09791460846264,     -1.0761933293368831 },
    std::array { 0.20350602217694591,     -1.0786011874516168 },
    std::array {  322.64465521384051,      -1.191406205950801 },
    std::array {  303.05274754093716,    -0.76402389817570904 },
    std::array {  125.54605341098205,     0.62123956039139938 },
    std::array {  255.55379336212889,     0.39372720787616516 },
    std::array {  25.253099635680755,     -1.2621683359761382 },
    std::array {  220.49844102877225,      1.0698034112156505 },
    std::array {   285.4113245844805,    -0.23110454718825935 },
    std::array {  75.640643078888871,    -0.80563835757376612 },
    std::array {  10.981269106853651,     -1.1176108685618158 },
    std::array {  72.200301544450113,    -0.56384661752830523 },
    std::array {  105.22905699135045,     0.36266570011403404 },
    std::array {  176.70673897641407,      2.5039869184510311 },
    std::array {  14.725592740051543,     -2.7516984016890631 },
    std::array {  94.745108006846181,     -1.0868860323404002 },
    std::array {  163.59732197840896,      1.9024498212176972 },
    std::array {   219.1140546860114,      2.1956031908457927 },
    std::array {  40.395670353607748,     -2.4448618547112297 },
    std::array {  130.80071114677799,     0.58739963327856204 },
    std::array {  157.03648746185482,      1.6654846892218929 },
    std::array {  207.46844781385195,      2.2826531243922052 },
    std::array {  328.74079125964016,     -1.2688632177822343 },
    std::array {  345.43305043978768,     -1.9549394957143078 },
    std::array {  4.1129472499411834,     -2.1236793589843246 },
    std::array {  215.47965023293602,     0.51814170707345664 },
    std::array {  47.064482142815294,    -0.38299257490959748 },
    std::array {  203.64080757694009,     0.56511980727284705 },
    std::array {  230.20037899408575,     0.33594923427089829 },
    std::array {  248.36831744293985,    0.061825498411869115 },
    std::array {  314.80920783544036,     -0.6583096943276544 },
    std::array {  337.22130045580701,    -0.76027962094663692 },
    std::array {  352.33289190979161,    -0.79460654684948318 },
    std::array {  7.1876638593435533,    -0.75070904682477813 },
    std::array {  45.411586715797675,    -0.33340827121724853 },
    std::array {  53.412606880961356,    -0.24907897472013393 },
    std::array {  58.300509841856403,    -0.17393000409668402 },
    std::array {  66.523719443545218,     -1.5578803926111751 },
    std::array {  171.70717063623931,      1.1052467698436592 },
    std::array {  245.73754224139768,      1.5612328323508939 },
    std::array {  259.25241954212373,      1.3837820634014821 },
    std::array {  267.66984568251416,      1.1738054947169323 },
    std::array {  303.19315778126116,     0.23501136191030025 },
    std::array {  315.50394930609559,    -0.15511997669928637 },
    std::array {  323.39645363629654,     -0.3439085107224113 },
    std::array {  331.21728438302068,    -0.62267578799761691 },
    std::array {  354.40564376731396,     -1.1686679402232547 },
    std::array {  357.48233604038239,     -1.2791201184691998 },
    std::array {  1.7694664885515892,     -1.3060082495717882 },
    std::array {  67.003503056877094,     -11.297047878198549 },
    std::array {  121.35824474872403,      3.1908450842296299 },
    std::array {  185.13880355394517,      16.078835497585093 },
    std::array {  199.20260523701927,      17.158806136226271 },
    std::array {  210.38970772769116,      16.419173662832623 },
    std::array {  251.45476722755618,      10.855239015039684 },
    std::array {  264.29112191709334,      7.4813486364181934 },
    std::array {  268.98985572553977,       6.301572725395042 },
    std::array {  276.96061209800632,      3.6436999982450673 },
    std::array {  299.38722668282281,     -2.3397855259638027 },
    std::array {  301.41007571448074,     -3.2848318471282014 },
    std::array {  305.00323355363554,     -3.9126360636727835 },
  };

  // NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
  constexpr std::array dataset {
    // Target                         Requested JDE(TT)       Independent RA (deg)       Independent Dec (deg)
    EquatorialRow { Target::SUN,         2412175.329074,  357.89439416553807,  -0.91328136116639358 },
    EquatorialRow { Target::SUN,         2429178.442349,  191.96752403913058,   -5.1385057219939059 },
    EquatorialRow { Target::SUN,         2441975.740939,  204.76393366582619,   -10.295088621512498 },
    EquatorialRow { Target::SUN,         2443886.721958,  294.21293013622761,   -21.574571890144444 },
    EquatorialRow { Target::SUN,         2445654.410718,  230.56273611062326,    -18.51501844573637 },
    EquatorialRow { Target::SUN,         2451545.000000,  281.27756491251142,   -23.032487195862934 },
    EquatorialRow { Target::SUN,         2453720.394735,  263.48185917753972,   -23.305382192506322 },
    EquatorialRow { Target::SUN,         2454673.347801,  125.53868507381486,    19.433308747439114 },
    EquatorialRow { Target::SUN,         2456183.746539,  171.59357196019434,    3.6259976291681069 },
    EquatorialRow { Target::SUN,         2460000.500000,   337.8803896134749,   -9.2714468051112604 },
    EquatorialRow { Target::SUN,         2460687.500000,  293.79230673362025,    -21.63762339437956 },
    EquatorialRow { Target::SUN,         2461120.000000,  359.89413534039534, -0.045789216402328366 },
    EquatorialRow { Target::MOON,        2412175.329074,  221.90404238649123,   -15.814306754395091 },
    EquatorialRow { Target::MOON,        2429178.442349,  343.09348904925997,   -2.1218914666681243 },
    EquatorialRow { Target::MOON,        2441975.740939,  135.03095718218739,    13.446534105978865 },
    EquatorialRow { Target::MOON,        2443886.721958,    112.622899131387,    17.479461992970979 },
    EquatorialRow { Target::MOON,        2445654.410718, 0.74894754859908486,   -5.2485042993374362 },
    EquatorialRow { Target::MOON,        2451545.000000,  222.44369513574932,   -10.897515592851871 },
    EquatorialRow { Target::MOON,        2453720.394735,  86.089386317950812,    28.253768985186639 },
    EquatorialRow { Target::MOON,        2454673.347801,  30.069920802168454,    17.687722684970936 },
    EquatorialRow { Target::MOON,        2456183.746539,  135.55807511694147,    11.768852462306652 },
    EquatorialRow { Target::MOON,        2460000.500000,  36.172905984883272,    14.614643845905203 },
    EquatorialRow { Target::MOON,        2460687.500000,  87.096048164993292,    28.444486747147192 },
    EquatorialRow { Target::MOON,        2461120.000000,  16.093621145362103,    10.498456651313429 },
    EquatorialRow { Target::MERCURY,     2412175.329074,  8.1313091230650318,    3.4964342411459803 },
    EquatorialRow { Target::MERCURY,     2429178.442349,  190.03530290333384,   -2.8154660441443604 },
    EquatorialRow { Target::MERCURY,     2441975.740939,  227.87715608046395,   -20.911654497390305 },
    EquatorialRow { Target::MERCURY,     2443886.721958,  276.81482283456046,    -23.79164937004758 },
    EquatorialRow { Target::MERCURY,     2445654.410718,  240.01463537128552,   -21.927838976410111 },
    EquatorialRow { Target::MERCURY,     2451545.000000,  272.07338593420161,   -24.418845074460478 },
    EquatorialRow { Target::MERCURY,     2453720.394735,  241.71032544032474,   -18.960229218774131 },
    EquatorialRow { Target::MERCURY,     2454673.347801,  121.05737211927456,    21.773556382065109 },
    EquatorialRow { Target::MERCURY,     2456183.746539,   174.3986932815001,     4.025666617658147 },
    EquatorialRow { Target::MERCURY,     2460000.500000,  323.22571807304058,   -16.637606552079841 },
    EquatorialRow { Target::MERCURY,     2460687.500000,  275.61452785899473,   -23.683764429767685 },
    EquatorialRow { Target::MERCURY,     2461120.000000,  339.72451367875476,   -7.3975098463668996 },
    EquatorialRow { Target::VENUS,       2412175.329074,  36.119876957177304,    15.545620753376133 },
    EquatorialRow { Target::VENUS,       2429178.442349,  232.15928187437464,    -24.23036120929385 },
    EquatorialRow { Target::VENUS,       2441975.740939,  250.34709147019885,    -25.02656507278062 },
    EquatorialRow { Target::VENUS,       2443886.721958,  244.45151042056932,   -17.573752350568967 },
    EquatorialRow { Target::VENUS,       2445654.410718,  186.82211797316637,   -1.3707266848064481 },
    EquatorialRow { Target::VENUS,       2451545.000000,  239.89185037118241,   -18.448722162743362 },
    EquatorialRow { Target::VENUS,       2453720.394735,  302.52527782106574,   -21.282217398451422 },
    EquatorialRow { Target::VENUS,       2454673.347801,  138.96989239416476,    17.381179327671553 },
    EquatorialRow { Target::VENUS,       2456183.746539,   129.3639795378682,    17.439214571128684 },
    EquatorialRow { Target::VENUS,       2460000.500000,  5.6002705696054411,    1.5055647326410488 },
    EquatorialRow { Target::VENUS,       2460687.500000,  340.95464153938838,   -8.5865443633286738 },
    EquatorialRow { Target::VENUS,       2461120.000000,  16.415735239071811,     6.038380896892213 },
    EquatorialRow { Target::MARS,        2412175.329074,  272.68424961824098,   -23.574522715817352 },
    EquatorialRow { Target::MARS,        2429178.442349,  169.82609009631841,    5.6642348269763474 },
    EquatorialRow { Target::MARS,        2441975.740939,  31.749051598081024,     10.57960889744334 },
    EquatorialRow { Target::MARS,        2443886.721958,  296.24565633475856,   -22.260434205847897 },
    EquatorialRow { Target::MARS,        2445654.410718,  179.22585382389914,    1.9613259188388446 },
    EquatorialRow { Target::MARS,        2451545.000000,  330.51625668200103,   -13.182681029834498 },
    EquatorialRow { Target::MARS,        2453720.394735,  35.660634514090177,    15.486581746403894 },
    EquatorialRow { Target::MARS,        2454673.347801,  166.18320482505044,    6.8636821756693598 },
    EquatorialRow { Target::MARS,        2456183.746539,  220.85106778968569,    -16.51903872414287 },
    EquatorialRow { Target::MARS,        2460000.500000,  76.222845746725909,    25.299407062003532 },
    EquatorialRow { Target::MARS,        2460687.500000,    120.861101629692,    24.727185199034803 },
    EquatorialRow { Target::MARS,        2461120.000000,  345.76871173055122,   -7.2499364447790828 },
    EquatorialRow { Target::JUPITER,     2412175.329074, 0.61603755719068987,  -0.90847116757314705 },
    EquatorialRow { Target::JUPITER,     2429178.442349,  325.39853963998689,   -15.096191739843405 },
    EquatorialRow { Target::JUPITER,     2441975.740939,  305.53401401434911,   -20.221899794303582 },
    EquatorialRow { Target::JUPITER,     2443886.721958,   128.0714170029494,    19.486222071758903 },
    EquatorialRow { Target::JUPITER,     2445654.410718,  254.36145244922045,   -22.267305529997138 },
    EquatorialRow { Target::JUPITER,     2451545.000000,  23.867859088059934,    8.5942908003663074 },
    EquatorialRow { Target::JUPITER,     2453720.394735,  218.42568091896288,   -13.955524028908693 },
    EquatorialRow { Target::JUPITER,     2454673.347801,  286.75173868288772,   -22.779370539374177 },
    EquatorialRow { Target::JUPITER,     2456183.746539,  74.502670456674039,    21.861761198177923 },
    EquatorialRow { Target::JUPITER,     2460000.500000,  10.532999466621803,    3.3169510760206897 },
    EquatorialRow { Target::JUPITER,     2460687.500000,   70.79311426193415,    21.695604939473476 },
    EquatorialRow { Target::JUPITER,     2461120.000000,   106.5713213304231,    22.929290389105212 },
    EquatorialRow { Target::SATURN,      2412175.329074,  177.97531413742587,    3.6075861470960038 },
    EquatorialRow { Target::SATURN,      2429178.442349,   14.62229414323259,    3.2662881642081594 },
    EquatorialRow { Target::SATURN,      2441975.740939,  95.127702392572346,    22.271580523687096 },
    EquatorialRow { Target::SATURN,      2443886.721958,  165.62437655567328,      8.20548037345627 },
    EquatorialRow { Target::SATURN,      2445654.410718,  217.43888442703064,   -12.452952592826016 },
    EquatorialRow { Target::SATURN,      2451545.000000,  38.765421627269603,    12.614766304961359 },
    EquatorialRow { Target::SATURN,      2453720.394735,   133.4226689488942,    18.090612182771046 },
    EquatorialRow { Target::SATURN,      2454673.347801,  159.38396866356067,    10.474364584589702 },
    EquatorialRow { Target::SATURN,      2456183.746539,  206.32838538226505,   -8.4397417533355021 },
    EquatorialRow { Target::SATURN,      2460000.500000,  331.33718640401355,   -13.101094764749313 },
    EquatorialRow { Target::SATURN,      2460687.500000,   347.3526280755168,   -7.5437018894005323 },
    EquatorialRow { Target::SATURN,      2461120.000000,  4.6173443542084103,  -0.31455431275347051 },
    EquatorialRow { Target::URANUS,      2412175.329074,  213.35627299390475,   -12.868670729707727 },
    EquatorialRow { Target::URANUS,      2429178.442349,   44.71086706121725,    16.568277154447891 },
    EquatorialRow { Target::URANUS,      2441975.740939,  202.09169482276411,   -8.6544993665470731 },
    EquatorialRow { Target::URANUS,      2443886.721958,  227.85185840772053,   -17.471005322403325 },
    EquatorialRow { Target::URANUS,      2445654.410718,  246.63438900947995,    -21.64239629281975 },
    EquatorialRow { Target::URANUS,      2451545.000000,   317.4748465174207,   -17.020305503457653 },
    EquatorialRow { Target::URANUS,      2453720.394735,  339.21610553074038,    -9.565696048427478 },
    EquatorialRow { Target::URANUS,      2454673.347801,  353.27335027559292,   -3.7722424344151491 },
    EquatorialRow { Target::URANUS,      2456183.746539,  6.8969618192325131,    2.1627878261130973 },
    EquatorialRow { Target::URANUS,      2460000.500000,  43.047384172659655,    16.136738296949616 },
    EquatorialRow { Target::URANUS,      2460687.500000,  51.089929057085875,    18.384471366241385 },
    EquatorialRow { Target::URANUS,      2461120.000000,   56.09483656954891,    19.611246875661113 },
    EquatorialRow { Target::NEPTUNE,     2412175.329074,   64.94738932334262,    19.877382196952212 },
    EquatorialRow { Target::NEPTUNE,     2429178.442349,  172.82054570739112,    4.3054196131937186 },
    EquatorialRow { Target::NEPTUNE,     2441975.740939,  244.12693192355087,   -19.728545167835438 },
    EquatorialRow { Target::NEPTUNE,     2443886.721958,   258.4312736727847,    -21.62497590700621 },
    EquatorialRow { Target::NEPTUNE,     2445654.410718,  267.48283732772529,   -22.247801801689285 },
    EquatorialRow { Target::NEPTUNE,     2451545.000000,  305.43299192236833,   -19.213196774582681 },
    EquatorialRow { Target::NEPTUNE,     2453720.394735,  318.01397555355595,   -16.336986818677783 },
    EquatorialRow { Target::NEPTUNE,     2454673.347801,  325.84341437238095,    -14.04562259625196 },
    EquatorialRow { Target::NEPTUNE,     2456183.746539,  333.47583503392116,   -11.622562347686662 },
    EquatorialRow { Target::NEPTUNE,     2460000.500000,  355.32839401920495,   -3.2952145435080871 },
    EquatorialRow { Target::NEPTUNE,     2460687.500000,  358.19854232614148,   -2.1748645949968743 },
    EquatorialRow { Target::NEPTUNE,     2461120.000000,   2.142801091226394,  -0.49461544676690633 },
    EquatorialRow { Target::PLUTO,       2412175.329074,  67.080521316842891,    10.345903257173534 },
    EquatorialRow { Target::PLUTO,       2429178.442349,  124.35649155300237,    22.973295325101262 },
    EquatorialRow { Target::PLUTO,       2441975.740939,    191.179852406984,    12.700749156431501 },
    EquatorialRow { Target::PLUTO,       2443886.721958,  204.20884313776148,    8.3758950076079781 },
    EquatorialRow { Target::PLUTO,       2445654.410718,  213.97811306781236,    3.8009710242153028 },
    EquatorialRow { Target::PLUTO,       2451545.000000,  251.41916985118556,   -11.394296485394229 },
    EquatorialRow { Target::PLUTO,       2453720.394735,   264.1156084598835,   -15.842932732968782 },
    EquatorialRow { Target::PLUTO,       2454673.347801,  268.94931956131626,   -17.134802134086922 },
    EquatorialRow { Target::PLUTO,       2456183.746539,  277.37671140959912,   -19.614765470521341 },
    EquatorialRow { Target::PLUTO,       2460000.500000,  302.06975215199367,   -22.566068681724126 },
    EquatorialRow { Target::PLUTO,       2460687.500000,  304.43249219326111,   -23.047162765316013 },
    EquatorialRow { Target::PLUTO,       2461120.000000,  308.37704433133899,   -22.809060858450231 },
  };
  // End retained data: equatorial-projection.
  // NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

  static_assert(provider_inputs.size() == dataset.size());
  for (std::size_t index = 0; index < dataset.size(); ++index) {
    const auto& [target, jde_tt, α, δ] = dataset.at(index);
    SCOPED_TRACE(::testing::Message() << "target=" << static_cast<uint32_t>(target) << " jde_tt=" << jde_tt);
    const auto& [λ, β] = provider_inputs.at(index);
    const auto ε = earth::obliquity::true_obliquity(jde_tt, earth::nutation::Model::IAU_1980);
    const auto projected = coords::ecliptic_to_equatorial(toolbox::AngleDeg { λ }, toolbox::AngleDeg { β }, ε);
    ASSERT_NEAR(std::remainder(projected.α.deg() - α, 360.0), 0.0, TOLERANCE_DEG);
    ASSERT_NEAR(projected.δ.deg(), δ, TOLERANCE_DEG);

    const auto live = position(target, jde_tt);
    const auto composed = coords::ecliptic_to_equatorial(live.longitude, live.latitude, ε);
    const auto result = equatorial_position(target, jde_tt);
    EXPECT_EQ(result.right_ascension.deg(), composed.α.deg());
    EXPECT_EQ(result.declination.deg(), composed.δ.deg());
  }
}

TEST(EquatorialQueryGolden, NodesVsIndependentDefinition) {
  // Independently transcribed Meeus, 2nd ed., (47.7) and p.344's five principal true-node terms;
  // (47.7)'s cubic denominator is 467441. Add SOFA iauNut80 delta-psi, rotate by iauObl80 plus
  // delta-epsilon as above. Descending nodes add 180 deg, latitude is zero, distance is absent.
  // Generated 2026-10-24 at seven fixed TT epochs. This checks the named mathematical definition,
  // not an observational/osculating-node ephemeris. The 1e-9 deg margin matches the projection check.
  constexpr double TOLERANCE_DEG = 1e-9;

  // NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
  // Retained data: equatorial-nodes.
  constexpr std::array dataset {
    // Target                                          JDE(TT)           Independent RA (deg)  Independent Dec (deg)
    EquatorialRow { Target::MEAN_ASCENDING,         2409543.875, 188.42020301657658, -3.6347246153041763 },
    EquatorialRow { Target::MEAN_DESCENDING,        2409543.875, 8.4202030165766057,  3.6347246153041777 },
    EquatorialRow { Target::TRUE_ASCENDING,         2409543.875, 188.20488593441928, -3.5426620738000798 },
    EquatorialRow { Target::TRUE_DESCENDING,        2409543.875, 8.2048859344193055,  3.5426620738000847 },
    EquatorialRow { Target::MEAN_ASCENDING,  2420000.1234567892, 355.83986545651788, -1.8026530087301151 },
    EquatorialRow { Target::MEAN_DESCENDING, 2420000.1234567892, 175.83986545651791,   1.802653008730102 },
    EquatorialRow { Target::TRUE_ASCENDING,  2420000.1234567892, 354.74982129980816, -2.2733584415409163 },
    EquatorialRow { Target::TRUE_DESCENDING, 2420000.1234567892, 174.74982129980816,  2.2733584415409234 },
    EquatorialRow { Target::MEAN_ASCENDING,           2433282.5, 11.138156593119172,  4.7894599427533304 },
    EquatorialRow { Target::MEAN_DESCENDING,          2433282.5, 191.13815659311919,  -4.789459942753334 },
    EquatorialRow { Target::TRUE_ASCENDING,           2433282.5, 11.494450798690764,  4.9399030203072334 },
    EquatorialRow { Target::TRUE_DESCENDING,          2433282.5, 191.49445079869076, -4.9399030203072334 },
    EquatorialRow { Target::MEAN_ASCENDING,           2451545.0, 127.39170951944216,  19.005375957494685 },
    EquatorialRow { Target::MEAN_DESCENDING,          2451545.0, 307.39170951944214, -19.005375957494685 },
    EquatorialRow { Target::TRUE_ASCENDING,           2451545.0, 126.24201725292514,  19.271945441862105 },
    EquatorialRow { Target::TRUE_DESCENDING,          2451545.0, 306.24201725292511, -19.271945441862115 },
    EquatorialRow { Target::MEAN_ASCENDING,  2460000.9876543209, 34.917765858907757,  13.936738124068793 },
    EquatorialRow { Target::MEAN_DESCENDING, 2460000.9876543209, 214.91776585890773, -13.936738124068791 },
    EquatorialRow { Target::TRUE_ASCENDING,  2460000.9876543209, 33.511238507532575,  13.460783222597172 },
    EquatorialRow { Target::TRUE_DESCENDING, 2460000.9876543209, 213.51123850753254, -13.460783222597172 },
    EquatorialRow { Target::MEAN_ASCENDING,          2475000.25, 325.34410106201062,  -13.84551333321221 },
    EquatorialRow { Target::MEAN_DESCENDING,         2475000.25, 145.34410106201065,  13.845513333212208 },
    EquatorialRow { Target::TRUE_ASCENDING,          2475000.25,  326.6184124363063, -13.413151710907163 },
    EquatorialRow { Target::TRUE_DESCENDING,         2475000.25,  146.6184124363063,  13.413151710907169 },
    EquatorialRow { Target::MEAN_ASCENDING,           2488069.0, 351.69831465245488, -3.5801502929849915 },
    EquatorialRow { Target::MEAN_DESCENDING,          2488069.0, 171.69831465245488,  3.5801502929849889 },
    EquatorialRow { Target::TRUE_ASCENDING,           2488069.0, 350.72571995931918, -3.9948353511929198 },
    EquatorialRow { Target::TRUE_DESCENDING,          2488069.0, 170.72571995931915,  3.9948353511929264 },
  };
  // NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

  for (const auto& [target, jde_tt, α, δ] : dataset) {
    SCOPED_TRACE(::testing::Message() << "target=" << static_cast<uint32_t>(target) << " jde_tt=" << jde_tt);
    const auto result = equatorial_position(target, jde_tt);
    ASSERT_NEAR(std::remainder(result.right_ascension.deg() - α, 360.0), 0.0, TOLERANCE_DEG);
    ASSERT_NEAR(result.declination.deg(), δ, TOLERANCE_DEG);
    ASSERT_FALSE(result.distance.has_value());
  }
}

} // namespace astro::ephemeris::test
