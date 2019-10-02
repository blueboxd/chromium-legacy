// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';

export class ThemeProxy {
  /**
   * @return {!Promise<!Object<string, string>>} Object with CSS variables
   *     as keys and rgba strings as values
   */
  getColors() {
    return sendWithPromise('getThemeColors');
  }

  startObserving() {
    chrome.send('observeThemeChanges');
  }
}

addSingletonGetter(ThemeProxy);
