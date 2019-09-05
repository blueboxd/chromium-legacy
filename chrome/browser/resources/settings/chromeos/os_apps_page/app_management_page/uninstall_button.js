// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-uninstall-button',

  behaviors: [
    I18nBehavior,
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * Returns true if the button should be disabled due to app install type.
   *
   * @param {App} app
   * @return {?boolean}
   * @private
   */
  getDisableState_: function(app) {
    if (!app) {
      return true;
    }

    switch (app.installSource) {
      case InstallSource.kSystem:
      case InstallSource.kPolicy:
        return true;
      case InstallSource.kOem:
      case InstallSource.kDefault:
      case InstallSource.kSync:
      case InstallSource.kUser:
      case InstallSource.kUnknown:
        return false;
      default:
        assertNotReached();
    }
  },

  /**
   * Returns string to be shown as a tool tip over the policy indicator.
   *
   * @param {App} app
   * @return {?string}
   * @private
   */
  getTooltip_: function(app) {
    if (!app) {
      return '';
    }

    switch (app.installSource) {
      case InstallSource.kSystem:
        return this.i18n('systemAppUninstallPolicy');
      case InstallSource.kPolicy:
        return this.i18n('policyAppUninstallPolicy');
      default:
        assertNotReached();
    }
  },

  /**
   * Returns true if the app was installed by a policy
   *
   * @param {App} app
   * @returns {boolean}
   * @private
   */
  showPolicyIndicator_: function(app) {
    if (!app) {
      return false;
    }
    return app.installSource === InstallSource.kPolicy ||
        app.installSource === InstallSource.kSystem;
  },

  /**
   * @private
   */
  onClick_: function() {
    app_management.BrowserProxy.getInstance().handler.uninstall(this.app_.id);
  },
});
