
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file exists to make tests work. Optimizing the NTP
 * flattens most JS files into a single new_tab_page.rollup.js. Therefore, tests
 * cannot import things from individual modules anymore. This file exports the
 * things tests need.
 */

import './app.js';

export {BrowserProxy} from './browser_proxy.js';
export {BackgroundSelectionType} from './customize_dialog.js';
export {$$, createScrollBorders, decodeString16, hexColorToSkColor, mojoString16, skColorToRgba} from './utils.js';
