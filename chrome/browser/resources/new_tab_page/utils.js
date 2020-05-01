// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Converts an SkColor object to a string in the form
 * "rgba(<red>, <green>, <blue>, <alpha>)".
 * @param {skia.mojom.SkColor} skColor The input color.
 * @return {string} The rgba string.
 */
export function skColorToRgba(skColor) {
  const a = (skColor.value >> 24) & 0xff;
  const r = (skColor.value >> 16) & 0xff;
  const g = (skColor.value >> 8) & 0xff;
  const b = skColor.value & 0xff;
  return `rgba(${r}, ${g}, ${b}, ${a})`;
}

/**
 * Converts a string of the form "#rrggbb" to an SkColor object.
 * @param {string} hexColor The color string.
 * @return {!skia.mojom.SkColor} The SkColor object,
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

/**
 * Given a |container| that has scrollable content, <div>'s before and after the
 * |container| are created with an attribute "scroll-border". These <div>'s are
 * updated to have an attribute "show" when there is more content in the
 * direction of the "scroll-border". Styling is left to the caller.
 *
 * Returns an |IntersectionObserver| so the caller can disconnect the observer
 * when needed.
 * @param {!HTMLElement} container
 * @return {!IntersectionObserver}
 */
export function createScrollBorders(container) {
  const topProbe = document.createElement('div');
  container.prepend(topProbe);
  const bottomProbe = document.createElement('div');
  container.append(bottomProbe);
  const topBorder = document.createElement('div');
  topBorder.toggleAttribute('scroll-border', true);
  container.parentNode.insertBefore(topBorder, container);
  const bottomBorder = document.createElement('div');
  bottomBorder.toggleAttribute('scroll-border', true);
  container.parentNode.insertBefore(bottomBorder, container.nextSibling);
  const observer = new IntersectionObserver(entries => {
    entries.forEach(({target, intersectionRatio}) => {
      const show = intersectionRatio === 0;
      if (target === topProbe) {
        topBorder.toggleAttribute('show', show);
      } else if (target === bottomProbe) {
        bottomBorder.toggleAttribute('show', show);
      }
    });
  }, {root: container});
  observer.observe(topProbe);
  observer.observe(bottomProbe);
  return observer;
}

/**
 * Converts a mojoBase.mojom.String16 to a JavaScript String.
 * @param {?mojoBase.mojom.String16} str
 * @return {string}
 */
export function decodeString16(str) {
  return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
}

/**
 * Converts a JavaScript String to a mojoBase.mojom.String16.
 * @param {string} str
 * @return {!mojoBase.mojom.String16}
 */
export function mojoString16(str) {
  const array = new Array(str.length);
  for (let i = 0; i < str.length; ++i) {
    array[i] = str.charCodeAt(i);
  }
  return {data: array};
}

/**
 * Converts a time delta in milliseconds to mojoBase.mojom.TimeDelta.
 * @param {number} timeDelta time delta in milliseconds
 * @returns {!mojoBase.mojom.TimeDelta}
 */
export function mojoTimeDelta(timeDelta) {
  return {microseconds: Math.floor(timeDelta * 1000)};
}

/**
 * Queries |selector| on |element|'s shadow root and returns the resulting
 * element if there is any.
 * @param {!Element} element
 * @param {string} selector
 * @return {Element}
 */
export function $$(element, selector) {
  return element.shadowRoot.querySelector(selector);
}
