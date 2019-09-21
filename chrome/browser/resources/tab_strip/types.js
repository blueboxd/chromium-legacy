// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for Tab Strip.
 */

/**
 * @typedef {{
 *    tabId: number,
 *    windowId: number,
 * }}
 */
let TabActivatedInfo;

/**
 * @typedef {{
 *    newPosition: number,
 *    newWindowId: number,
 * }}
 */
let TabAttachedInfo;

/**
 * @typedef {{
 *    oldPosition: number,
 *    oldWindowId: number,
 * }}
 */
let TabDetachedInfo;

/**
 * @typedef {{
 *    fromIndex: number,
 *    toIndex: number,
 *    windowId: number,
 * }}
 */
let TabMovedInfo;

/**
 * @typedef {{
 *    isWindowClosing: boolean,
 *    windowId: number,
 * }}
 */
let WindowRemoveInfo;
