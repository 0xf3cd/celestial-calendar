# Delta T Source Attribution

This record identifies the sources of three Delta T implementations retained by celestial-calendar.
It records attribution only. It does not assert that attribution supplies permission, that the source
material is covered by the project's licence, or that any upstream material has been relicensed.

## Algorithm 1

- Author: Xu Jianwei.
- Work: `寿星万年历2008版(V1.3.2)`, dated 2008-08-31.
- Source: https://web.archive.org/web/20080919020456id_/http://www.fjptsz.com/xxjs/xjw/rj/115.htm
- Project relation: the 19 populated coefficient rows match the source. The project locally moves the
  source's `2014/2114` continuation anchors to `2015/2115` so the retained segment boundaries meet.

The former cnblogs link is a later republication and is not used as the source identity.

## Algorithm 3

- Author and work: Fred Espenak, `Thousand Year Canon of Solar Eclipses 1501 to 2500` (2014).
- Exact expressions: https://www.eclipsewise.com/help/deltatpoly2014.html
- Project relation: the two post-2005 expressions are transcribed from this source. The source credits
  the quadratic trend to Marc van der Sluys.

The project does not claim that EclipseWise's permission for eclipse data applies to these expressions
in software.

## Algorithm 5 Long-Term Branch

- Authors: L. V. Morrison, F. R. Stephenson, C. Y. Hohenkerk, and M. Zawilski.
- Addendum: https://doi.org/10.1098/rspa.2020.0776
- Corrected combined expression:
  https://web.archive.org/web/20230103030546id_/https://astro.ukho.gov.uk/nao/lvm/
- Project relation: the branch uses the corrected `1.72 / 3.5 / 14` expression and its analytic
  integral, anchored by the separately granted project-authored fitted constant.

The 2016 article's `1.78 / 4.0 / 15` expression is different and is not the implemented source. The
project does not describe the corrected expression as MIT, CC BY, OGL, permission-granted, or
relicensed.
