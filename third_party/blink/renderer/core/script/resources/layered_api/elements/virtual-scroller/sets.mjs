/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Utility functions for set operations.
 * @package
 */

/*
 * Returns the set of elements in |a| that are not in |b|.
 *
 * @param {!Set} a A set of elements.
 * @param {!Set} b A set of elements.
*/
export function difference(a, b) {
  const result = new Set();
  for (const element of a) {
    if (!b.has(element)) {
      result.add(element);
    }
  }
  return result;
}
