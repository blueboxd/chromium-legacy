// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A collection of helper functions when dealing with rects. */
const RectHelper = {
  /** @type {!chrome.accessibilityPrivate.ScreenRect} */
  ZERO_RECT: {top: 0, left: 0, width: 0, height: 0},

  /**
   * Finds the bottom of a rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {number}
   */
  bottom: (rect) => rect.top + rect.height,

  /**
   * Finds the right of a rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {number}
   */
  right: (rect) => rect.left + rect.width,

  /**
   * Returns the point at the center of the rectangle.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {!{x: number, y: number}} an object containing the x and y
   *     coordinates of the center.
   */
  center: (rect) => {
    const x = rect.left + Math.round(rect.width / 2);
    const y = rect.top + Math.round(rect.height / 2);
    return {x, y};
  },

  /**
   * Returns the union of the specified rectangles.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect1
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect2
   * @return {!chrome.accessibilityPrivate.ScreenRect}
   */
  union: (rect1, rect2) => {
    const top = rect1.top < rect2.top ? rect1.top : rect2.top;
    const left = rect1.left < rect2.left ? rect1.left : rect2.left;

    const r1Bottom = RectHelper.bottom(rect1);
    const r2Bottom = RectHelper.bottom(rect2);
    const bottom = r1Bottom > r2Bottom ? r1Bottom : r2Bottom;

    const r1Right = RectHelper.right(rect1);
    const r2Right = RectHelper.right(rect2);
    const right = r1Right > r2Right ? r1Right : r2Right;

    const height = bottom - top;
    const width = right - left;

    return {top, left, width, height};
  },

  /**
   * Returns the union of all the rectangles specified.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @return {!chrome.accessibilityPrivate.ScreenRect}
   */
  unionAll: (rects) => {
    if (rects.length < 1) return RectHelper.ZERO_RECT;

    let result = rects[0];
    for (let i = 1; i < rects.length; i++) {
      result = RectHelper.union(result, rects[i]);
    }
    return result;
  },

  /**
   * Returns true if the two rects are equal.
   *
   * @param {chrome.accessibilityPrivate.ScreenRect=} rect1
   * @param {chrome.accessibilityPrivate.ScreenRect=} rect2
   * @return {boolean}
   */
  areEqual: (rect1, rect2) => {
    if (!rect1 && !rect2) return true;
    if (!rect1 || !rect2) return false;
    return rect1.left === rect2.left && rect1.top === rect2.top &&
           rect1.width === rect2.width && rect1.height === rect2.height;
  },

  /**
   * Returns a string representing the given rectangle.
   * @param {chrome.accessibilityPrivate.ScreenRect|undefined} rect
   * @return {string}
   */
  toString: (rect) => {
    let str = '';
    if (rect) {
      str = rect.left + ',' + rect.top + ' ';
      str += rect.width + 'x' + rect.height;
    }
    return str;
  },

  /**
   * Increases the size of |outer| to entirely enclose |inner|, with |padding|
   * buffer on each side.
   * @param {number} padding
   * @param {chrome.accessibilityPrivate.ScreenRect=} outer
   * @param {chrome.accessibilityPrivate.ScreenRect=} inner
   * @return {chrome.accessibilityPrivate.ScreenRect|undefined}
   */
  expandToFitWithPadding: (padding, outer, inner) => {
    if (!outer || !inner)
      return outer;

    let newOuter = RectHelper.deepCopy(outer);

    if (newOuter.top > inner.top - padding) {
      newOuter.top = inner.top - padding;
      // The height should be the original bottom point less the new top point.
      newOuter.height = RectHelper.bottom(outer) - newOuter.top;
    }
    if (newOuter.left > inner.left - padding) {
      newOuter.left = inner.left - padding;
      // The new width should be the original right point less the new left.
      newOuter.width = RectHelper.right(outer) - newOuter.left;
    }
    if (RectHelper.bottom(newOuter) < RectHelper.bottom(inner) + padding) {
      newOuter.height = RectHelper.bottom(inner) + padding - newOuter.top;
    }
    if (RectHelper.right(newOuter) < RectHelper.right(inner) + padding) {
      newOuter.width = RectHelper.right(inner) + padding - newOuter.left;
    }

    return newOuter;
  },

  /**
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {!chrome.accessibilityPrivate.ScreenRect}
   */
  deepCopy: (rect) => {
    const copy = (Object.assign({}, rect));
    return /** @type {!chrome.accessibilityPrivate.ScreenRect} */ (copy);
  }

};
