// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-view-header',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /** @type {App} */
    app_: {
      type: Object,
    },
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   *
   * Returns True if the uninstall button should be disabled due to app install
   * type.
   *
   * @param {App} app
   * @return {boolean}
   * @private
   */
  getUninstallButtonDisableState_: function(app) {
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
   * Returns string to be shown as a tool tip over the uninstall button.
   *
   * @param {App} app
   * @return {string}
   * @private
   */
  getUninstallButtonHoverText_: function(app) {
    // TODO(crbug.com/957795) Replace strings and add them into i18n.
    switch (app.installSource) {
      case InstallSource.kSystem:
        return app.title + ' cannot be uninstalled as it is part of Chrome OS.';
      case InstallSource.kPolicy:
        return app.title + ' cannot be uninstalled as it has been' +
            ' installed by your administrator.';
      case InstallSource.kOem:
      case InstallSource.kDefault:
      case InstallSource.kSync:
      case InstallSource.kUser:
      case InstallSource.kUnknown:
        return `Click to uninstall ${app.title}.`;
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
  isPolicyApp_: function(app) {
    return app.installSource === InstallSource.kPolicy;
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return app_management.util.getAppIcon(app);
  },

  /**
   * @private
   */
  onClickBackButton_: function() {
    if (!window.history.state) {
      this.dispatch(app_management.actions.changePage(PageType.MAIN));
    } else {
      window.history.back();
    }
  },

  /**
   * @private
   */
  onClickUninstallButton_: function() {
    app_management.BrowserProxy.getInstance().handler.uninstall(this.app_.id);
  },
});
