// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-app-detail-view',

  behaviors: [
    app_management.StoreClient,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * @type {App}
     * @private
     */
    app_: {
      type: Object,
    },

    /**
     * @type {AppMap}
     * @private
     */
    apps_: {
      type: Object,
      observer: 'appsChanged_',
    },

    /**
     * @type {string}
     * @private
     */
    selectedAppId_: {
      type: String,
      observer: 'selectedAppIdChanged_',
    },
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.watch('apps_', state => state.apps);
    this.watch('selectedAppId_', state => state.selectedAppId);
    this.updateFromStore();
  },

  detached: function() {
    this.dispatch(app_management.actions.updateSelectedAppId(null));
  },

  /**
   * Updates selected app ID based on the URL query params.
   *
   * settings.RouteObserverBehavior
   * @param {!settings.Route} currentRoute
   * @protected
   */
  currentRouteChanged: function(currentRoute) {
    if (currentRoute !== settings.routes.APP_MANAGEMENT_DETAIL) {
      return;
    }

    if (this.selectedAppNotFound_()) {
      this.async(() => {
        app_management.util.openMainPage();
      });
      return;
    }

    const appId = settings.getQueryParameters().get('id');

    this.dispatch(app_management.actions.updateSelectedAppId(appId));
  },

  /**
   * @param {?App} app
   * @return {?string}
   * @private
   */
  getSelectedRouteId_: function(app) {
    if (!app) {
      return null;
    }

    const selectedAppType = app.type;
    switch (selectedAppType) {
      case (AppType.kWeb):
        return 'pwa-detail-view';
      case (AppType.kExtension):
        return 'chrome-app-detail-view';
      case (AppType.kArc):
        return 'arc-detail-view';
      default:
        assertNotReached();
    }
  },

  selectedAppIdChanged_: function(appId) {
    if (appId && this.app_) {
      app_management.util.recordAppManagementUserAction(
          this.app_.type, AppManagementUserAction.ViewOpened);
    }
  },

  /**
   * @private
   */
  appsChanged_: function() {
    if (this.selectedAppNotFound_()) {
      this.async(() => {
        app_management.util.openMainPage();
      });
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  selectedAppNotFound_: function() {
    const appId = settings.getQueryParameters().get('id');
    return this.apps_ && !this.apps_[appId];
  },
});
