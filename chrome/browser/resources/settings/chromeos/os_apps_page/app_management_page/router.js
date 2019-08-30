// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-router',

  behaviors: [
    app_management.StoreClient,
  ],
  //TODO (crbug.com/999443): Watch URL and update state.
  properties: {
    currentPage_: {
      type: Object,
      observer: 'onCurrentPageChanged_',
    },
  },

  attached: function() {
    this.watch('currentPage_', state => {
      return state.currentPage;
    });
    this.updateFromStore();
  },

  onCurrentPageChanged_: function() {
    const pageType = this.currentPage_.pageType;
    const appId = this.currentPage_.selectedAppId;
    switch(pageType) {
      case PageType.DETAIL:
        const params = new URLSearchParams;
        params.append('id', appId);
        settings.navigateTo(settings.routes.APP_MANAGEMENT_DETAIL, params);
        return;
      case PageType.MAIN:
        settings.navigateTo(settings.routes.APP_MANAGEMENT);
        return;
      default:
        assertNotReached();
    }
  }
});
