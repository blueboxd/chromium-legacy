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

  behaviors: [
    app_management.StoreClient,
  ],

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

    /**
     * @type {App}
     * @private
     */
    app_: Object,
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    if (!app) {
      return '';
    }
    return app_management.util.getAppIcon(app);
  },

  /** @private */
  onClickAppManagement_: function() {
    chrome.metricsPrivate.recordUserAction('SettingsPage.OpenAppManagement');
    settings.navigateTo(settings.routes.APP_MANAGEMENT);
  },
});
