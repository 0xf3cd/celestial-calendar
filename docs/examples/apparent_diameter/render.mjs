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

/** Draw fixed-instant scalar disks after the caller has initialized its installed package. */
export function renderDiameters(celestial, container) {
  const arcsecondsPerPixel = 10;
  const civil = { year: 2026, month: 11, day: 7, fraction: 0 };
  const disks = ["sun", "moon", "jupiter"].map((target) => celestial.diameter.query(target, civil, { scale: "utc" }));
  container.replaceChildren();
  container.style.cssText = "display:flex;flex-wrap:wrap;gap:2rem;padding:1.5rem;background:#e9f0f7;color:#172c42;font-family:system-ui,sans-serif";
  for (const disk of disks) {
    const magnification = disk.target === "jupiter" ? 20 : 1;
    const pixels = disk.diameterArcsec / arcsecondsPerPixel * magnification;
    const figure = document.createElement("figure");
    figure.style.cssText = "margin:0;display:grid;align-content:start;gap:1rem;min-width:12rem";
    const circle = document.createElement("div");
    circle.className = "apparent-disk";
    circle.dataset.target = disk.target;
    circle.dataset.arcseconds = String(disk.diameterArcsec);
    circle.dataset.magnification = String(magnification);
    circle.style.cssText = `width:${pixels}px;height:${pixels}px;border-radius:50%;background:#355c80;box-sizing:border-box`;
    const caption = document.createElement("figcaption");
    caption.style.fontFamily = "ui-monospace,monospace";
    caption.textContent = `${disk.target}: ${disk.diameterArcsec.toFixed(3)} arcsec${magnification === 1 ? "" : " (20x magnified)"}`;
    figure.append(caption, circle);
    container.append(figure);
  }
  return { arcsecondsPerPixel, targets: disks.map((disk) => disk.target) };
}
