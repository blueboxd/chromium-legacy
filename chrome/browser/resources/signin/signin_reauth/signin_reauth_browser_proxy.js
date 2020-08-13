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
   * Called when the app has been initialized.
   */
  initialize() {}

  /**
   * Called when the user confirms the signin reauth dialog.
   * @param {!Array<string>} description Strings that the user was presented
   *     with in the UI.
   * @param {string} confirmation Text of the element that the user
   *     clicked on.
   */
  confirm(description, confirmation) {}

  /**
   * Called when the user cancels the signin reauth.
   */
  cancel() {}
}

/** @implements {SigninReauthBrowserProxy} */
export class SigninReauthBrowserProxyImpl {
  /** @override */
  initialize() {
    chrome.send('initialize');
  }

  /** @override */
  confirm(description, confirmation) {
    chrome.send('confirm', [description, confirmation]);
  }

  /** @override */
  cancel() {
    chrome.send('cancel');
  }
}

addSingletonGetter(SigninReauthBrowserProxyImpl);
