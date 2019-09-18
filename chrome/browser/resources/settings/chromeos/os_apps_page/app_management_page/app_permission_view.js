// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  // TODO(crbug.com/999016): change to app-management-app-detail-view.
  is: 'app-management-app-permission-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @type {App}
     * @private
     */
    app_: Object,
  },

  attached: function() {
    if (!this.app_) {
      const appId = settings.getQueryParameters().get('id');
      // TODO(crbug.com/999443): move this changePage call to router.js
      this.dispatch(app_management.actions.changePage(PageType.DETAIL, appId));
    }
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.watch('currentPage_', state => state.currentPage);
    this.updateFromStore();
  },

  /**
   * @param {App} app
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
        return 'pwa-permission-view';
      case (AppType.kExtension):
        return 'chrome-app-permission-view';
      case (AppType.kArc):
        return 'arc-permission-view';
      default:
        assertNotReached();
    }
  },
});
