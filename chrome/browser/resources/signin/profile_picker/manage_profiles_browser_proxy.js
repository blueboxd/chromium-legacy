// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class ManageProfilesBrowserProxy {
  /**
   * Initializes profile picker main view.
   */
  initializeMainView() {}
}

/** @implements {ManageProfilesBrowserProxy} */
export class ManageProfilesBrowserProxyImpl {
  /** @override */
  initializeMainView() {
    chrome.send('mainViewInitialize');
  }
}

addSingletonGetter(ManageProfilesBrowserProxyImpl);
