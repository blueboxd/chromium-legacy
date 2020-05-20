// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the signin reauth dialog to
 * interact with the browser.
 */
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class SigninReauthBrowserProxy {
  /**
   * Called when the user confirms the signin reauth dialog.
   */
  confirm() {}

  /**
   * Called when the user cancels the signin reauth.
   */
  cancel() {}
}

/** @implements {SigninReauthBrowserProxy} */
export class SigninReauthBrowserProxyImpl {
  /** @override */
  confirm() {
    chrome.send('confirm');
  }

  /** @override */
  cancel() {
    chrome.send('cancel');
  }
}

addSingletonGetter(SigninReauthBrowserProxyImpl);
