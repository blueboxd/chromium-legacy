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
    assert(apps[action.app.id]);

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
    } else if (action.pageType === PageType.NOTIFICATIONS) {
      return {
        pageType: PageType.NOTIFICATIONS,
        selectedAppId: null,
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

  const SearchState = {};

  /**
   * @param {AppMap} apps
   * @param {SearchState} search
   * @param {Object} action
   * @return {SearchState}
   */
  SearchState.startSearch = function(apps, search, action) {
    if (action.term === search.term) {
      return search;
    }

    const results = [];

    for (const app of Object.values(apps)) {
      if (app.title.toLowerCase().includes(action.term.toLowerCase())) {
        results.push(app);
      }
    }

    results.sort(
        (a, b) => app_management.util.alphabeticalSort(
            assert(a.title), assert(b.title)));

    return /** @type {SearchState} */ (Object.assign({}, search, {
      term: action.term,
      results: results,
    }));
  };

  /** @return {SearchState} */
  SearchState.clearSearch = function() {
    return {
      term: null,
      results: null,
    };
  };

  /**
   * @param {AppMap} apps
   * @param {SearchState} search
   * @param {Object} action
   * @return {SearchState}
   */
  SearchState.updateSearch = function(apps, search, action) {
    switch (action.name) {
      case 'start-search':
        return SearchState.startSearch(apps, search, action);
      case 'clear-search':
      case 'change-page':
        return SearchState.clearSearch();
      default:
        return search;
    }
  };

  const NotificationsState = {};

  /**
   * @param {NotificationsState} notifications
   * @param {Object} action
   * @return {NotificationsState}
   */
  NotificationsState.addApp = function(notifications, action) {
    let {allowedIds, blockedIds} = notifications;
    const allowed = app_management.util.notificationsAllowed(action.app);

    if (allowed === OptionalBool.kUnknown) {
      return {allowedIds, blockedIds};
    }

    if (allowed === OptionalBool.kTrue) {
      allowedIds = app_management.util.addIfNeeded(allowedIds, action.app.id);
    } else {
      blockedIds = app_management.util.addIfNeeded(blockedIds, action.app.id);
    }

    return {allowedIds, blockedIds};
  };

  /**
   * @param {NotificationsState} notifications
   * @param {Object} action
   * @return {NotificationsState}
   */
  NotificationsState.changeApp = function(notifications, action) {
    let {allowedIds, blockedIds} = notifications;
    const allowed = app_management.util.notificationsAllowed(action.app);
    const id = action.app.id;

    if (allowed === OptionalBool.kUnknown) {
      assert(!blockedIds.has(id) && !allowedIds.has(id));
      return {allowedIds, blockedIds};
    }

    if (allowed === OptionalBool.kTrue) {
      allowedIds = app_management.util.addIfNeeded(allowedIds, id);
      blockedIds = app_management.util.removeIfNeeded(blockedIds, id);
    } else {
      allowedIds = app_management.util.removeIfNeeded(allowedIds, id);
      blockedIds = app_management.util.addIfNeeded(blockedIds, id);
    }

    return {allowedIds, blockedIds};
  };

  /**
   * @param {NotificationsState} notifications
   * @param {Object} action
   * @return {NotificationsState}
   */
  NotificationsState.removeApp = function(notifications, action) {
    let {allowedIds, blockedIds} = notifications;
    allowedIds = app_management.util.removeIfNeeded(allowedIds, action.id);
    blockedIds = app_management.util.removeIfNeeded(blockedIds, action.id);

    return {allowedIds, blockedIds};
  };

  /**
   * @param {NotificationsState} notifications
   * @param {Object} action
   * @return {NotificationsState}
   */
  NotificationsState.updateNotifications = function(notifications, action) {
    switch (action.name) {
      case 'add-app':
        return NotificationsState.addApp(notifications, action);
      case 'change-app':
        return NotificationsState.changeApp(notifications, action);
      case 'remove-app':
        return NotificationsState.removeApp(notifications, action);
      default:
        return notifications;
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
      search: SearchState.updateSearch(state.apps, state.search, action),
      notifications:
          NotificationsState.updateNotifications(state.notifications, action),
    };
  }

  return {
    reduceAction: reduceAction,
    AppState: AppState,
    CurrentPageState: CurrentPageState,
    ArcSupported: ArcSupported,
    NotificationsState: NotificationsState,
    SearchState: SearchState,
  };
});
