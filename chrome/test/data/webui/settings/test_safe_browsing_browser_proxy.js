// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SafeBrowsingBrowserProxy, SafeBrowsingRadioManagedState} from 'chrome://settings/lazy_load.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {SafeBrowsingBrowserProxy} */
export class TestSafeBrowsingBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getSafeBrowsingRadioManagedState',
      'validateSafeBrowsingEnhanced',
    ]);

    /** @type {!SafeBrowsingRadioManagedState} */
    this.managedRadioState_;
  }

  /** @param {!SafeBrowsingRadioManagedState} state */
  setSafeBrowsingRadioManagedState(state) {
    this.managedRadioState_ = state;
  }

  /** @override */
  getSafeBrowsingRadioManagedState() {
    this.methodCalled('getSafeBrowsingRadioManagedState');
    return Promise.resolve(this.managedRadioState_);
  }

  /** @override */
  validateSafeBrowsingEnhanced() {
    this.methodCalled('validateSafeBrowsingEnhanced');
  }
}
