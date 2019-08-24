// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-item',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * The name of the permission, to be displayed to the user.
     * @type {string}
     */
    permissionLabel: String,

    /**
     * A string version of the permission type. Must be a value of the
     * permission type enum corresponding to the AppType of app_.
     * E.g. A value of PwaPermissionType if app_.type === AppType.kWeb.
     * @type {string}
     */
    permissionType: String,

    /**
     * @type {App}
     */
    app_: Object,

    /**
     * @type {string}
     */
    icon: String,

    /**
     * True if the permission type is available for the app.
     * @private
     */
    available_: {
      type: Boolean,
      computed: 'isAvailable_(app_, permissionType)',
      reflectToAttribute: true,
    },
  },

  listeners: {
    'click': 'onClick_',
  },

  /**
   * Returns true if the permission type is available for the app.
   *
   * @param {App} app
   * @param {string} permissionType
   * @private
   */
  isAvailable_: function(app, permissionType) {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    assert(app);

    return app_management.util.getPermission(app, permissionType) !== undefined;
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @param {MouseEvent} e
   * @private
   */
  onClick_: function(e) {
    e.preventDefault();

    const toggle = /** @type {AppManagementPermissionToggleElement} */ (
        assert(this.$$('#permission-toggle')));
    toggle.togglePermission_();
  },
});
