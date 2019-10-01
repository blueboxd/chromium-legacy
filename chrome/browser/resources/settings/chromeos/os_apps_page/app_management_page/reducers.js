// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module of functions which produce a new page state in response
 * to an action. Reducers (in the same sense as Array.prototype.reduce) must be
 * pure functions: they must not modify existing state objects, or make any API
 * calls.
 */

cr.define('app_management', function() {
  const AppState = {};

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {AppMap}
   */
  AppState.addApp = function(apps, action) {
    assert(!apps[action.app.id]);

    const newAppEntry = {};
    newAppEntry[action.app.id] = action.app;
    return Object.assign({}, apps, newAppEntry);
  };

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {AppMap}
   */
  AppState.changeApp = function(apps, action) {
    // If the app doesn't exist, that means that the app that has been changed
    // does not need to be shown in App Management.
    if (!apps[action.app.id]) {
      return apps;
    }

    const changedAppEntry = {};
    changedAppEntry[action.app.id] = action.app;
    return Object.assign({}, apps, changedAppEntry);
  };

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {AppMap}
   */
  AppState.removeApp = function(apps, action) {
    if (!apps.hasOwnProperty(action.id)) {
      return apps;
    }

    delete apps[action.id];
    return Object.assign({}, apps);
  };

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {AppMap}
   */
  AppState.updateApps = function(apps, action) {
    switch (action.name) {
      case 'add-app':
        return AppState.addApp(apps, action);
      case 'change-app':
        return AppState.changeApp(apps, action);
      case 'remove-app':
        return AppState.removeApp(apps, action);
      default:
        return apps;
    }
  };

  const CurrentPageState = {};

  /**
   * @param {AppMap} apps
   * @param {Object} action
   * @return {Page}
   */
  CurrentPageState.changePage = function(apps, action) {
    if (action.pageType === PageType.DETAIL && apps[action.id]) {
      return {
        pageType: PageType.DETAIL,
        selectedAppId: action.id,
      };
    } else {
      return {
        pageType: PageType.MAIN,
        selectedAppId: null,
      };
    }
  };

  /**
   * @param {Page} currentPage
   * @param {Object} action
   * @return {Page}
   */
  CurrentPageState.removeApp = function(currentPage, action) {
    if (currentPage.pageType === PageType.DETAIL &&
        currentPage.selectedAppId === action.id) {
      return {
        pageType: PageType.MAIN,
        selectedAppId: null,
      };
    } else {
      return currentPage;
    }
  };

  /**
   * @param {AppMap} apps
   * @param {Page} currentPage
   * @param {Object} action
   * @return {Page}
   */
  CurrentPageState.updateCurrentPage = function(apps, currentPage, action) {
    switch (action.name) {
      case 'change-page':
        return CurrentPageState.changePage(apps, action);
      case 'remove-app':
        return CurrentPageState.removeApp(currentPage, action);
      default:
        return currentPage;
    }
  };

  const ArcSupported = {};

  /**
   * @param {boolean} arcSupported
   * @param {Object} action
   * @return {boolean}
   */
  ArcSupported.updateArcSupported = function(arcSupported, action) {
    switch (action.name) {
      case 'update-arc-supported':
        return action.value;
      default:
        return arcSupported;
    }
  };

  /**
   * Root reducer for the App Management page. This is called by the store in
   * response to an action, and the return value is used to update the UI.
   * @param {!AppManagementPageState} state
   * @param {Object} action
   * @return {!AppManagementPageState}
   */
  function reduceAction(state, action) {
    return {
      apps: AppState.updateApps(state.apps, action),
      currentPage: CurrentPageState.updateCurrentPage(
          state.apps, state.currentPage, action),
      arcSupported: ArcSupported.updateArcSupported(state.arcSupported, action),
    };
  }

  return {
    reduceAction: reduceAction,
    AppState: AppState,
    CurrentPageState: CurrentPageState,
    ArcSupported: ArcSupported,
  };
});
