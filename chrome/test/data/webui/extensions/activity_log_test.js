// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-activity-log. */
suite('ExtensionsActivityLogTest', function() {
  /**
   * Backing extension id, same id as the one in
   * extension_test_util.createExtensionInfo
   * @type {string}
   */
  const EXTENSION_ID = 'a'.repeat(32);

  /**
   * Extension activityLog created before each test.
   * @type {extensions.ActivityLog}
   */
  let activityLog;

  /**
   * Backing extension info for the activity log.
   * @type {chrome.developerPrivate.ExtensionInfo|
   *        extensions.ActivityLogExtensionPlaceholder}
   */
  let extensionInfo;

  let proxyDelegate;
  let testVisible;

  const testActivities = {activities: []};

  const activity1 = {
    extensionId: EXTENSION_ID,
    activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
    time: 1550101623113,
    args: JSON.stringify([null]),
    apiCall: 'testAPI.testMethod',
  };

  // Initialize an extension activity log before each test.
  setup(function() {
    PolymerTest.clearBody();

    activityLog = new extensions.ActivityLog();
    testVisible = extension_test_util.testVisible.bind(null, activityLog);

    extensionInfo = extension_test_util.createExtensionInfo({
      id: EXTENSION_ID,
    });
    activityLog.extensionInfo = extensionInfo;

    proxyDelegate = new extensions.TestService();
    activityLog.delegate = proxyDelegate;
    proxyDelegate.testActivities = testActivities;
    document.body.appendChild(activityLog);

    activityLog.fire('view-enter-start');

    // Wait until we have finished making the call to fetch the activity log.
    return proxyDelegate.whenCalled('getExtensionActivityLog');
  });

  teardown(function() {
    activityLog.remove();
  });

  // Returns a list of visible stream items. The not([hidden]) selector is
  // needed for iron-list as it reuses components but hides them when not in
  // use.
  function getStreamItems() {
    return activityLog.$$('activity-log-stream')
        .shadowRoot.querySelectorAll('activity-log-stream-item:not([hidden])');
  }

  test('clicking on back button navigates to the details page', function() {
    Polymer.dom.flush();

    let currentPage = null;
    extensions.navigation.addListener(newPage => {
      currentPage = newPage;
    });

    activityLog.$$('#closeButton').click();
    expectDeepEquals(
        currentPage,
        {page: extensions.Page.DETAILS, extensionId: EXTENSION_ID});
  });

  test(
      'clicking on back button for a placeholder page navigates to list view',
      function() {
        activityLog.extensionInfo = {id: EXTENSION_ID, isPlaceholder: true};

        Polymer.dom.flush();

        let currentPage = null;
        extensions.navigation.addListener(newPage => {
          currentPage = newPage;
        });

        activityLog.$$('#closeButton').click();
        expectDeepEquals(currentPage, {page: extensions.Page.LIST});
      });

  test('tab transitions', async () => {
    Polymer.dom.flush();
    // Default view should be the history view.
    testVisible('activity-log-history', true);

    // Navigate to the activity log stream.
    activityLog.$$('cr-tabs').selected = 1;
    Polymer.dom.flush();

    // One activity is recorded and should appear in the stream.
    proxyDelegate.getOnExtensionActivity().callListeners(activity1);

    Polymer.dom.flush();
    testVisible('activity-log-stream', true);
    expectEquals(1, getStreamItems().length);

    // Navigate back to the activity log history tab.
    activityLog.$$('cr-tabs').selected = 0;

    // Expect a refresh of the activity log.
    await proxyDelegate.whenCalled('getExtensionActivityLog');
    Polymer.dom.flush();
    testVisible('activity-log-history', true);

    // Another activity is recorded, but should not appear in the stream as
    // the stream is inactive.
    proxyDelegate.getOnExtensionActivity().callListeners(activity1);

    activityLog.$$('cr-tabs').selected = 1;
    Polymer.dom.flush();

    // The one activity in the stream should have persisted between tab
    // switches.
    expectEquals(1, getStreamItems().length);
  });
});
