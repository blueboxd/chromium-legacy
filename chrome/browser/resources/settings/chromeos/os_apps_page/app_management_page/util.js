// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for the App Management page.
 */

cr.define('app_management.util', function() {
  /**
   * @return {!AppManagementPageState}
   */
  function createEmptyState() {
    return {
      apps: {},
      currentPage: {
        pageType: PageType.MAIN,
        selectedAppId: null,
      },
      arcSupported: false,
    };
  }

  /**
   * @param {!Array<App>} apps
   * @return {!AppManagementPageState}
   */
  function createInitialState(apps) {
    const initialState = createEmptyState();

    initialState.arcSupported =
        loadTimeData.valueExists('isSupportedArcVersion') &&
        loadTimeData.getBoolean('isSupportedArcVersion');

    for (const app of apps) {
      initialState.apps[app.id] = app;
    }

    return initialState;
  }

  /**
   * @param {number} permissionId
   * @param {!PermissionValueType} valueType
   * @param {number} value
   * @param {boolean} isManaged
   * @return {!Permission}
   */
  function createPermission(permissionId, valueType, value, isManaged) {
    return {
      permissionId,
      valueType,
      value,
      isManaged,
    };
  }

  /**
   * @param {App} app
   * @return {string}
   */
  function getAppIcon(app) {
    return `chrome://app-icon/${app.id}/128`;
  }

  /**
   * If the given value is not in the set, returns a new set with the value
   * added, otherwise returns the old set.
   * @template T
   * @param {!Set<T>} set
   * @param {T} value
   * @return {!Set<T>}
   */
  function addIfNeeded(set, value) {
    if (!set.has(value)) {
      set = new Set(set);
      set.add(value);
    }
    return set;
  }

  /**
   * If the given value is in the set, returns a new set without the value,
   * otherwise returns the old set.
   * @template T
   * @param {!Set<T>} set
   * @param {T} value
   * @return {!Set<T>}
   */
  function removeIfNeeded(set, value) {
    if (set.has(value)) {
      set = new Set(set);
      set.delete(value);
    }
    return set;
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  function getPermissionValueBool(app, permissionType) {
    const permission = getPermission(app, permissionType);
    assert(permission);

    switch (permission.valueType) {
      case PermissionValueType.kBool:
        return permission.value === Bool.kTrue;
      case PermissionValueType.kTriState:
        return permission.value === TriState.kAllow;
      default:
        assertNotReached();
    }
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {Permission}
   */
  function getPermission(app, permissionType) {
    return app.permissions[permissionTypeHandle(app, permissionType)];
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {number}
   */
  function permissionTypeHandle(app, permissionType) {
    switch (app.type) {
      case AppType.kWeb:
        return PwaPermissionType[permissionType];
      case AppType.kArc:
        return ArcPermissionType[permissionType];
      default:
        assertNotReached();
    }
  }

  /**
   * @param {AppManagementPageState} state
   * @return {?App}
   */
  function getSelectedApp(state) {
    const selectedAppId = state.currentPage.selectedAppId;
    return selectedAppId ? state.apps[selectedAppId] : null;
  }

  /**
   * A comparator function to sort strings alphabetically.
   *
   * @param {string} a
   * @param {string} b
   */
  function alphabeticalSort(a, b) {
    return a.localeCompare(b);
  }

  /**
   * Toggles an OptionalBool
   *
   * @param {OptionalBool} bool
   * @return {OptionalBool}
   */
  function toggleOptionalBool(bool) {
    switch (bool) {
      case OptionalBool.kFalse:
        return OptionalBool.kTrue;
      case OptionalBool.kTrue:
        return OptionalBool.kFalse;
      default:
        assertNotReached();
    }
  }

  /**
   * @param {OptionalBool} optionalBool
   * @returns {boolean}
   */
  function convertOptionalBoolToBool(optionalBool) {
    switch (optionalBool) {
      case OptionalBool.kTrue:
        return true;
      case OptionalBool.kFalse:
        return false;
      default:
        assertNotReached();
    }
  }

  return {
    addIfNeeded: addIfNeeded,
    alphabeticalSort: alphabeticalSort,
    convertOptionalBoolToBool: convertOptionalBoolToBool,
    createEmptyState: createEmptyState,
    createInitialState: createInitialState,
    createPermission: createPermission,
    getAppIcon: getAppIcon,
    getPermission: getPermission,
    getPermissionValueBool: getPermissionValueBool,
    getSelectedApp: getSelectedApp,
    permissionTypeHandle: permissionTypeHandle,
    removeIfNeeded: removeIfNeeded,
    toggleOptionalBool: toggleOptionalBool,
  };
});
