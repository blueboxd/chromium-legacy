// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the Chrome captions section to
 * interact with the browser. Used on operating system that is not Chrome OS.
 */

// clang-format off
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
export class CaptionsBrowserProxy {
  /**
   * Open the native captions system dialog.
   */
  openSystemCaptionsDialog() {}
}

/**
 * @implements {CaptionsBrowserProxy}
 */
export class CaptionsBrowserProxyImpl {
  /** @override */
  openSystemCaptionsDialog() {
    chrome.send('openSystemCaptionsDialog');
  }
}

addSingletonGetter(CaptionsBrowserProxyImpl);
