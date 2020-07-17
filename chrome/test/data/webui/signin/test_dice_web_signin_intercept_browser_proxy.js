// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

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
