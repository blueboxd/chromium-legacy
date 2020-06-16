// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {account_manager.AccountManagerBrowserProxy} */
class TestAccountManagerBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'closeDialog',
      'reauthenticateAccount',
    ]);
  }

  /** @override */
  closeDialog() {
    this.methodCalled('closeDialog');
  }

  /** @override */
  reauthenticateAccount(account_email) {
    this.methodCalled('reauthenticateAccount', account_email);
  }
}
