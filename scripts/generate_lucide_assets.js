#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

const icons = [
  ["radio_tower", "radio-tower"],
  ["wifi", "wifi"],
  ["wifi_off", "wifi-off"],
  ["signal_high", "signal-high"],
  ["signal_medium", "signal-medium"],
  ["signal_low", "signal-low"],
  ["signal_zero", "signal-zero"],
  ["power", "power"],
  ["sun", "sun"],
  ["sparkles", "sparkles"],
  ["send", "send"],
  ["check", "check"],
  ["locate_fixed", "locate-fixed"],
  ["refresh_cw", "refresh-cw"],
  ["rotate_ccw", "rotate-ccw"],
  ["triangle_alert", "triangle-alert"],
  ["battery", "battery"],
  ["battery_charging", "battery-charging"],
];

const outDir = process.argv[2] || path.join(process.cwd(), "build", "generated");
const iconDir = path.join(process.cwd(), "node_modules", "lucide-static", "icons");
const sizes = [16, 88];
const viewBox = 24;
const sourceStroke = 2;

function readIcon(slug) {
  const file = path.join(iconDir, `${slug}.svg`);
  if (!fs.existsSync(file)) {
    throw new Error(`Missing lucide icon: ${file}. Run npm ci first.`);
  }
  return fs.readFileSync(file, "utf8");
}

function attr(tag, name) {
  const match = tag.match(new RegExp(`${name}="([^"]+)"`));
  return match ? match[1] : null;
}

function tokenizePath(d) {
  return d.match(/[a-zA-Z]|[-+]?(?:\d*\.\d+|\d+)(?:e[-+]?\d+)?/g) || [];
}

function cubic(p0, p1, p2, p3, t) {
  const mt = 1 - t;
  return mt * mt * mt * p0 + 3 * mt * mt * t * p1 + 3 * mt * t * t * p2 + t * t * t * p3;
}

function arcToSegments(x1, y1, rx, ry, angle, largeArc, sweep, x2, y2) {
  if (rx === 0 || ry === 0) {
    return [[[x1, y1], [x2, y2]]];
  }

  const phi = (angle * Math.PI) / 180;
  const cosPhi = Math.cos(phi);
  const sinPhi = Math.sin(phi);
  const dx = (x1 - x2) / 2;
  const dy = (y1 - y2) / 2;
  let x1p = cosPhi * dx + sinPhi * dy;
  let y1p = -sinPhi * dx + cosPhi * dy;
  rx = Math.abs(rx);
  ry = Math.abs(ry);

  const lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
  if (lambda > 1) {
    const scale = Math.sqrt(lambda);
    rx *= scale;
    ry *= scale;
  }

  const rx2 = rx * rx;
  const ry2 = ry * ry;
  const x1p2 = x1p * x1p;
  const y1p2 = y1p * y1p;
  const sign = largeArc === sweep ? -1 : 1;
  const coef = sign * Math.sqrt(Math.max(0, (rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2) / (rx2 * y1p2 + ry2 * x1p2)));
  const cxp = coef * (rx * y1p) / ry;
  const cyp = coef * (-ry * x1p) / rx;
  const cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) / 2;
  const cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) / 2;

  function angleBetween(ux, uy, vx, vy) {
    const dot = ux * vx + uy * vy;
    const len = Math.sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
    const sign = ux * vy - uy * vx < 0 ? -1 : 1;
    return sign * Math.acos(Math.min(1, Math.max(-1, dot / len)));
  }

  const ux = (x1p - cxp) / rx;
  const uy = (y1p - cyp) / ry;
  const vx = (-x1p - cxp) / rx;
  const vy = (-y1p - cyp) / ry;
  let theta1 = angleBetween(1, 0, ux, uy);
  let delta = angleBetween(ux, uy, vx, vy);
  if (!sweep && delta > 0) delta -= 2 * Math.PI;
  if (sweep && delta < 0) delta += 2 * Math.PI;

  const steps = Math.max(6, Math.ceil(Math.abs(delta) / (Math.PI / 18)));
  const points = [];
  for (let i = 0; i <= steps; i++) {
    const t = theta1 + (delta * i) / steps;
    const x = cx + rx * Math.cos(t) * cosPhi - ry * Math.sin(t) * sinPhi;
    const y = cy + rx * Math.cos(t) * sinPhi + ry * Math.sin(t) * cosPhi;
    points.push([x, y]);
  }

  const segments = [];
  for (let i = 1; i < points.length; i++) {
    segments.push([points[i - 1], points[i]]);
  }
  return segments;
}

function pathSegments(d) {
  const tokens = tokenizePath(d);
  let i = 0;
  let cmd = null;
  let x = 0;
  let y = 0;
  let sx = 0;
  let sy = 0;
  const segments = [];

  const isCommand = (v) => /^[a-zA-Z]$/.test(v);
  const num = () => Number(tokens[i++]);
  const hasNumber = () => i < tokens.length && !isCommand(tokens[i]);

  while (i < tokens.length) {
    if (isCommand(tokens[i])) {
      cmd = tokens[i++];
    }
    if (!cmd) break;

    const relative = cmd === cmd.toLowerCase();
    const c = cmd.toUpperCase();

    if (c === "M") {
      x = (relative ? x : 0) + num();
      y = (relative ? y : 0) + num();
      sx = x;
      sy = y;
      while (hasNumber()) {
        const nx = (relative ? x : 0) + num();
        const ny = (relative ? y : 0) + num();
        segments.push([[x, y], [nx, ny]]);
        x = nx;
        y = ny;
      }
    } else if (c === "L") {
      while (hasNumber()) {
        const nx = (relative ? x : 0) + num();
        const ny = (relative ? y : 0) + num();
        segments.push([[x, y], [nx, ny]]);
        x = nx;
        y = ny;
      }
    } else if (c === "H") {
      while (hasNumber()) {
        const nx = (relative ? x : 0) + num();
        segments.push([[x, y], [nx, y]]);
        x = nx;
      }
    } else if (c === "V") {
      while (hasNumber()) {
        const ny = (relative ? y : 0) + num();
        segments.push([[x, y], [x, ny]]);
        y = ny;
      }
    } else if (c === "C") {
      while (hasNumber()) {
        const x1 = (relative ? x : 0) + num();
        const y1 = (relative ? y : 0) + num();
        const x2 = (relative ? x : 0) + num();
        const y2 = (relative ? y : 0) + num();
        const x3 = (relative ? x : 0) + num();
        const y3 = (relative ? y : 0) + num();
        let px = x;
        let py = y;
        for (let step = 1; step <= 24; step++) {
          const t = step / 24;
          const nx = cubic(x, x1, x2, x3, t);
          const ny = cubic(y, y1, y2, y3, t);
          segments.push([[px, py], [nx, ny]]);
          px = nx;
          py = ny;
        }
        x = x3;
        y = y3;
      }
    } else if (c === "A") {
      while (hasNumber()) {
        const rx = num();
        const ry = num();
        const angle = num();
        const largeArc = num();
        const sweep = num();
        const nx = (relative ? x : 0) + num();
        const ny = (relative ? y : 0) + num();
        segments.push(...arcToSegments(x, y, rx, ry, angle, largeArc, sweep, nx, ny));
        x = nx;
        y = ny;
      }
    } else if (c === "Z") {
      segments.push([[x, y], [sx, sy]]);
      x = sx;
      y = sy;
    } else {
      throw new Error(`Unsupported SVG path command ${cmd} in "${d}"`);
    }
  }

  return segments;
}

function circleSegments(cx, cy, r) {
  const points = [];
  const steps = 48;
  for (let i = 0; i <= steps; i++) {
    const t = (Math.PI * 2 * i) / steps;
    points.push([cx + Math.cos(t) * r, cy + Math.sin(t) * r]);
  }
  const segments = [];
  for (let i = 1; i < points.length; i++) {
    segments.push([points[i - 1], points[i]]);
  }
  return segments;
}

function parseSvg(svg) {
  const segments = [];
  for (const tag of svg.match(/<path\b[^>]*>/g) || []) {
    segments.push(...pathSegments(attr(tag, "d")));
  }
  for (const tag of svg.match(/<line\b[^>]*>/g) || []) {
    segments.push([[
      Number(attr(tag, "x1")),
      Number(attr(tag, "y1")),
    ], [
      Number(attr(tag, "x2")),
      Number(attr(tag, "y2")),
    ]]);
  }
  for (const tag of svg.match(/<circle\b[^>]*>/g) || []) {
    segments.push(...circleSegments(Number(attr(tag, "cx")), Number(attr(tag, "cy")), Number(attr(tag, "r"))));
  }
  return segments;
}

function distanceToSegment(px, py, ax, ay, bx, by) {
  const dx = bx - ax;
  const dy = by - ay;
  const length2 = dx * dx + dy * dy;
  if (length2 === 0) return Math.hypot(px - ax, py - ay);
  const t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / length2));
  return Math.hypot(px - (ax + t * dx), py - (ay + t * dy));
}

function rasterize(segments, size) {
  const bytes = new Uint8Array(Math.ceil((size * size) / 2));
  const scale = size / viewBox;
  const radius = (sourceStroke * scale) / 2;
  const samples = size <= 16 ? 5 : 4;

  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      let hits = 0;
      for (let sy = 0; sy < samples; sy++) {
        for (let sx = 0; sx < samples; sx++) {
          const px = x + (sx + 0.5) / samples;
          const py = y + (sy + 0.5) / samples;
          for (const [[ax, ay], [bx, by]] of segments) {
            if (distanceToSegment(px, py, ax * scale, ay * scale, bx * scale, by * scale) <= radius) {
              hits++;
              break;
            }
          }
        }
      }
      const pixel = y * size + x;
      const alpha = Math.round((hits * 15) / (samples * samples));
      if ((pixel & 1) === 0) {
        bytes[pixel >> 1] |= alpha;
      } else {
        bytes[pixel >> 1] |= alpha << 4;
      }
    }
  }

  return bytes;
}

function bytesLiteral(bytes) {
  const parts = [];
  for (let i = 0; i < bytes.length; i++) {
    parts.push(`0x${bytes[i].toString(16).padStart(2, "0")}`);
  }
  const lines = [];
  for (let i = 0; i < parts.length; i += 12) {
    lines.push(`    ${parts.slice(i, i + 12).join(", ")},`);
  }
  return lines.join("\n");
}

fs.mkdirSync(outDir, { recursive: true });

let header = `#pragma once

#include <stdint.h>

typedef struct {
    uint8_t size;
    const uint8_t *alpha4;
} status_display_icon_asset_t;

const status_display_icon_asset_t *status_display_icon_get_16(int icon);
const status_display_icon_asset_t *status_display_icon_get_88(int icon);
`;

let source = `#include "status_display_icons.h"

#include "status_display.h"

`;

for (const [name, slug] of icons) {
  const segments = parseSvg(readIcon(slug));
  for (const size of sizes) {
    const bytes = rasterize(segments, size);
    source += `static const uint8_t s_${name}_${size}[] = {\n${bytesLiteral(bytes)}\n};\n\n`;
  }
}

source += "static const status_display_icon_asset_t s_icons_16[] = {\n";
for (const [name] of icons) {
  source += `    {16, s_${name}_16},\n`;
}
source += "};\n\nstatic const status_display_icon_asset_t s_icons_88[] = {\n";
for (const [name] of icons) {
  source += `    {88, s_${name}_88},\n`;
}
source += `};

const status_display_icon_asset_t *status_display_icon_get_16(int icon)
{
    if (icon < 0 || icon >= STATUS_DISPLAY_ICON_COUNT) {
        return 0;
    }
    return &s_icons_16[icon];
}

const status_display_icon_asset_t *status_display_icon_get_88(int icon)
{
    if (icon < 0 || icon >= STATUS_DISPLAY_ICON_COUNT) {
        return 0;
    }
    return &s_icons_88[icon];
}
`;

fs.writeFileSync(path.join(outDir, "status_display_icons.h"), header);
fs.writeFileSync(path.join(outDir, "status_display_icons.c"), source);
