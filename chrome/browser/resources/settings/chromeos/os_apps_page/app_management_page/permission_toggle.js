// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-toggle',

  properties: {
    /**
     * @type {App}
     */
    app: Object,

    /**
     * A string version of the permission type. Must be a value of the
     * permission type enum corresponding to the AppType of app_.
     * E.g. A value of PwaPermissionType if app_.type === AppType.kWeb.
     * @type {string}
     */
    permissionType: String,
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  getPermissionValueBool_: function(app, permissionType) {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    assert(app);

    return app_management.util.getPermissionValueBool(app, permissionType);
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  isPermissionManaged_: function(app, permissionType) {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    assert(app);

    const permission = app_management.util.getPermission(app, permissionType);
    assert(permission);
    return permission.isManaged;
  },

  togglePermission_: function() {
    assert(this.app);

    /** @type {!Permission} */
    let newPermission;

    switch (app_management.util.getPermission(this.app, this.permissionType)
                .valueType) {
      case PermissionValueType.kBool:
        newPermission =
            this.getNewPermissionBoolean_(this.app, this.permissionType);
        break;
      case PermissionValueType.kTriState:
        newPermission =
            this.getNewPermissionTriState_(this.app, this.permissionType);
        break;
      default:
        assertNotReached();
    }

    app_management.BrowserProxy.getInstance().handler.setPermission(
        this.app.id, newPermission);
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {!Permission}
   * @private
   */
  getNewPermissionBoolean_: function(app, permissionType) {
    let newPermissionValue;
    const currentPermission =
        app_management.util.getPermission(app, permissionType);

    switch (currentPermission.value) {
      case Bool.kFalse:
        newPermissionValue = Bool.kTrue;
        break;
      case Bool.kTrue:
        newPermissionValue = Bool.kFalse;
        break;
      default:
        assertNotReached();
    }

    assert(newPermissionValue !== undefined);
    return app_management.util.createPermission(
        app_management.util.permissionTypeHandle(app, permissionType),
        PermissionValueType.kBool, newPermissionValue,
        currentPermission.isManaged);
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {!Permission}
   * @private
   */
  getNewPermissionTriState_: function(app, permissionType) {
    let newPermissionValue;
    const currentPermission =
        app_management.util.getPermission(app, permissionType);

    switch (currentPermission.value) {
      case TriState.kBlock:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAsk:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAllow:
        // TODO(rekanorman): Eventually TriState.kAsk, but currently changing a
        // permission to kAsk then opening the site settings page for the app
        // produces the error:
        // "Only extensions or enterprise policy can change the setting to ASK."
        newPermissionValue = TriState.kBlock;
        break;
      default:
        assertNotReached();
    }

    assert(newPermissionValue !== undefined);
    return app_management.util.createPermission(
        app_management.util.permissionTypeHandle(app, permissionType),
        PermissionValueType.kTriState, newPermissionValue,
        currentPermission.isManaged);
  },
});
