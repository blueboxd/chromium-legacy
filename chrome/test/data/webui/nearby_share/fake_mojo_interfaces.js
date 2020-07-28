// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Contains fake implementations of mojo interfaces. */

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/**
 * @implements {nearbyShare.mojom.ConfirmationManagerInterface}
 * @extends {TestBrowserProxy}
 */
export class FakeConfirmationManagerRemote extends TestBrowserProxy {
  constructor() {
    super([
      'accept',
      'reject',
    ]);
  }

  async accept() {
    this.methodCalled('accept');
    return {success: true};
  }

  async reject() {
    this.methodCalled('reject');
    return {success: true};
  }
}

/**
 * @implements {nearbyShare.mojom.DiscoveryManagerInterface}
 * @extends {TestBrowserProxy}
 */
export class FakeDiscoveryManagerRemote extends TestBrowserProxy {
  constructor() {
    super([
      'selectShareTarget',
      'startDiscovery',
    ]);

    this.selectShareTargetResult = {
      result: nearbyShare.mojom.SelectShareTargetResult.kOk,
      token: null,
      confirmationManager: null,
    };
  }

  /**
   * @param {!mojoBase.mojom.UnguessableToken} shareTargetId
   * @suppress {checkTypes} FakeConfirmationManagerRemote does not extend
   * ConfirmationManagerRemote but implements ConfirmationManagerInterface.
   */
  async selectShareTarget(shareTargetId) {
    this.methodCalled('selectShareTarget', shareTargetId);
    return this.selectShareTargetResult;
  }

  async startDiscovery(listener) {
    this.methodCalled('startDiscovery', listener);
    return {success: true};
  }
}
