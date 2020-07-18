// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiceWebSigninInterceptBrowserProxy} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {DiceWebSigninInterceptBrowserProxy} */
export class TestDiceWebSigninInterceptBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['accept', 'cancel']);
  }

  /** @override */
  accept() {
    this.methodCalled('accept');
  }

  /** @override */
  cancel() {
    this.methodCalled('cancel');
  }
}
