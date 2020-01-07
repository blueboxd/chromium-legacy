// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Converts an SkColor object to a string in the form
 * "rgb(<red>, <green>, <blue>)".
 * Note: Can only convert opaque SkColors.
 * @param {skia.mojom.SkColor} skColor The input color.
 * @return {string} The rgb string.
 */
export function skColorToRgb(skColor) {
  // Bail on transparent colors.
  if (skColor.value < 0xff000000) {
    return 'rgb(0, 0, 0)';
  }
  const r = (skColor.value >> 16) & 0xff;
  const g = (skColor.value >> 8) & 0xff;
  const b = skColor.value & 0xff;
  return `rgb(${r}, ${g}, ${b})`;
}

/**
 * Converts a string of the form "#rrggbb" to an SkColor object.
 * @param {string} hexColor The color string.
 * @return {skia.mojom.SkColor} The SkColor object,
 */
export function hexColorToSkColor(hexColor) {
  if (!/^#[0-9a-f]{6}$/.test(hexColor)) {
    return {value: 0};
  }
  const r = parseInt(hexColor.substring(1, 3), 16);
  const g = parseInt(hexColor.substring(3, 5), 16);
  const b = parseInt(hexColor.substring(5, 7), 16);
  return {value: 0xff000000 + (r << 16) + (g << 8) + b};
}
