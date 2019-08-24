// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-search-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * List of apps returned from search results.
     * @private {Array<App>}
     */
    apps_: {
      type: Array,
      value: () => [],
    },
  },

  attached: function() {
    this.watch('apps_', (state) => {
      return state.search.results;
    });
  },

  /**
   * Check whether there is search results.
   * @param {Array<App>} apps
   * @return {boolean}
   * @private
   */
  isEmptyList_: function(apps) {
    if (!apps) {
      return true;
    }
    return apps.length === 0;
  },
});
