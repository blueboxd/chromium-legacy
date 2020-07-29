// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AccountInfo, DiceWebSigninInterceptBrowserProxy} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {DiceWebSigninInterceptBrowserProxy} */
export class TestDiceWebSigninInterceptBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['accept', 'cancel', 'pageLoaded']);
    /** @private {!AccountInfo} */
    this.accountInfo_ = {pictureUrl: '', name: ''};
  }

  /** @param {!AccountInfo} info */
  setAccountInfo(info) {
    this.accountInfo_ = info;
  }

  /** @override */
  accept() {
    this.methodCalled('accept');
  }

  /** @override */
  cancel() {
    this.methodCalled('cancel');
  }

  /** @override */
  pageLoaded() {
    this.methodCalled('pageLoaded');
    return Promise.resolve(this.accountInfo_);
  }
}
