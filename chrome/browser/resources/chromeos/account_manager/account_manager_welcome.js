// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('account_manager_welcome', function() {
  'use strict';

  Polymer({
    is: 'account-manager-welcome',

    /** @private */
    closeDialog_() {
      account_manager.AccountManagerBrowserProxyImpl.getInstance()
          .closeDialog();
    },
  });
});
