// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const
 */
chrome.resourcesPrivate = {};

/**
 * Records a value than can range from 1 to 1,000,000.
 * @param {string} component
 * @param {!function(Object)} callback Callback to handle the dictionary of
 *     localized strings for |component|.
 */
chrome.resourcesPrivate.getStrings = function(component, callback) {};
