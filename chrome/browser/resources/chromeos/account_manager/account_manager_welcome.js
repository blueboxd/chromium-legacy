// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('account_manager_welcome', function() {
  'use strict';

  function initialize() {
    $('ok-button').addEventListener('click', closeDialog);
  }

  function closeDialog() {
    account_manager.AccountManagerBrowserProxyImpl.getInstance().closeDialog();
  }

  return {
    initialize: initialize,
  };
});

document.addEventListener(
    'DOMContentLoaded', account_manager_welcome.initialize);
