// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-app-item',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /** @type {App} */
    app: {
      type: Object,
    },
  },

  listeners: {
    'click': 'onClick_',
  },

  /**
   * @private
   */
  onClick_: function() {
    app_management.util.openAppDetailPage(this.app.id);
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return app_management.util.getAppIcon(app);
  },
});
