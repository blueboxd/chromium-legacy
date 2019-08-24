// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-metadata-view',

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
   * @param {App} app
   * @return bool
   * @private
   */
  pinToShelfToggleVisible_: function(app) {
    return app.isPinned !== OptionalBool.kUnknown;
  },

  /**
   * Returns a bool representation of the app's isPinned value, used to
   * determine the position of the "Pin to Shelf" toggle.
   * @param {App} app
   * @return bool
   * @private
   */
  isPinned_: function(app) {
    return app.isPinned === OptionalBool.kTrue;
  },

  isPolicyPinned_: function(app) {
    return app.isPolicyPinned === OptionalBool.kTrue;
  },

  /** @private */
  togglePinned_: function() {
    let newPinnedValue;

    switch (this.app_.isPinned) {
      case OptionalBool.kFalse:
        newPinnedValue = OptionalBool.kTrue;
        break;
      case OptionalBool.kTrue:
        newPinnedValue = OptionalBool.kFalse;
        break;
      default:
        assertNotReached();
    }

    app_management.BrowserProxy.getInstance().handler.setPinned(
        this.app_.id, assert(newPinnedValue));
  },

  /**
   * @param {App} app
   * @return {?string}
   * @private
   */
  versionString_: function(app) {
    if (!app.version) {
      return null;
    }

    return loadTimeData.getStringF('version', assert(app.version));
  },

  /**
   * @param {App} app
   * @return {?string}
   * @private
   */
  sizeString_: function(app) {
    if (!app.size) {
      return null;
    }

    return loadTimeData.getStringF('size', assert(app.size));
  },
});
