// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-apps-page' is the settings page containing app related settings.
 *
 */
Polymer({
  is: 'os-settings-apps-page',

  properties: {
    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.APP_MANAGEMENT) {
          map.set(settings.routes.APP_MANAGEMENT.path, '#appManagement');
        }
        return map;
      },
    },

  },

  /** @private */
  onClickAppManagement_: function() {
    chrome.metricsPrivate.recordUserAction('SettingsPage.OpenAppManagement');
    settings.navigateTo(settings.routes.APP_MANAGEMENT);
  },
});
