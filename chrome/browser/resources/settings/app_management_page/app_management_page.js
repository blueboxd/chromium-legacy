// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-management-page' is the settings page which links to App Management.
 */

Polymer({
  is: 'settings-app-management-page',

  /** @private */
  openAppManagement_: function() {
    chrome.metricsPrivate.recordUserAction('SettingsPage.OpenAppManagement');
    settings.OpenWindowProxyImpl.getInstance().openURL(
        'chrome://app-management');
  },
});
