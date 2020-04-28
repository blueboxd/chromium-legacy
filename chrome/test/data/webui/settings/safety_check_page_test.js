// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {HatsBrowserProxyImpl, LifetimeBrowserProxyImpl, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerProxy, Router, routes, SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckExtensionsStatus, SafetyCheckInteractions, SafetyCheckParentStatus, SafetyCheckPasswordsStatus, SafetyCheckSafeBrowsingStatus, SafetyCheckUpdatesStatus} from 'chrome://settings/settings.js';
import {TestHatsBrowserProxy} from 'chrome://test/settings/test_hats_browser_proxy.js';
import {TestLifetimeBrowserProxy} from 'chrome://test/settings/test_lifetime_browser_proxy.m.js';
import {TestMetricsBrowserProxy} from 'chrome://test/settings/test_metrics_browser_proxy.js';
import {TestOpenWindowProxy} from 'chrome://test/settings/test_open_window_proxy.js';
import {TestPasswordManagerProxy} from 'chrome://test/settings/test_password_manager_proxy.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

// clang-format on

suite('SafetyCheckUiTests', function() {
  /** @type {?LifetimeBrowserProxy} */
  let lifetimeBrowserProxy = null;
  /** @type {settings.TestMetricsBrowserProxy} */
  let metricsBrowserProxy;
  /** @type {OpenWindowProxy} */
  let openWindowProxy = null;
  /** @type {SafetyCheckBrowserProxy} */
  let safetyCheckBrowserProxy = null;
  /** @type {SettingsBasicPageElement} */
  let page;

  setup(function() {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = metricsBrowserProxy;
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.instance_ = openWindowProxy;
    safetyCheckBrowserProxy =
        TestBrowserProxy.fromClass(SafetyCheckBrowserProxy);
    safetyCheckBrowserProxy.setResultFor(
        'getParentRanDisplayString', Promise.resolve('Dummy string'));
    SafetyCheckBrowserProxyImpl.instance_ = safetyCheckBrowserProxy;

    PolymerTest.clearBody();
    page = document.createElement('settings-safety-check-page');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  function fireSafetyCheckParentEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    webUIListenerCallback(SafetyCheckCallbackConstants.PARENT_CHANGED, event);
  }

  function fireSafetyCheckUpdatesEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    webUIListenerCallback(SafetyCheckCallbackConstants.UPDATES_CHANGED, event);
  }

  function fireSafetyCheckPasswordsEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    event.passwordsButtonString = null;
    webUIListenerCallback(
        SafetyCheckCallbackConstants.PASSWORDS_CHANGED, event);
  }

  function fireSafetyCheckSafeBrowsingEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    webUIListenerCallback(
        SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED, event);
  }

  function fireSafetyCheckExtensionsEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    webUIListenerCallback(
        SafetyCheckCallbackConstants.EXTENSIONS_CHANGED, event);
  }

  function assertIconStatusRunning(icon) {
    assertTrue(icon.classList.contains('icon-blue'));
    assertFalse(icon.classList.contains('icon-red'));
    assertEquals('Running', icon.getAttribute('aria-label'));
  }

  function assertIconStatusSafe(icon) {
    assertTrue(icon.classList.contains('icon-blue'));
    assertFalse(icon.classList.contains('icon-red'));
    assertEquals('Passed', icon.getAttribute('aria-label'));
  }

  function assertIconStatusInfo(icon) {
    assertFalse(icon.classList.contains('icon-blue'));
    assertFalse(icon.classList.contains('icon-red'));
    assertEquals('Info', icon.getAttribute('aria-label'));
  }

  function assertIconStatusWarning(icon) {
    assertFalse(icon.classList.contains('icon-blue'));
    assertTrue(icon.classList.contains('icon-red'));
    assertEquals('Warning', icon.getAttribute('aria-label'));
  }

  /**
   * @return {!Promise}
   */
  async function expectExtensionsButtonClickActions() {
    // User clicks review extensions button.
    page.$$('#safetyCheckExtensionsButton').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.SAFETY_CHECK_EXTENSIONS_REVIEW,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ReviewExtensions',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    assertEquals(
        'chrome://extensions', await openWindowProxy.whenCalled('openURL'));
  }

  /** Tests parent element and collapse.from start to completion */
  test('testParentAndCollapse', async function() {
    // Before the check, only the text button is present.
    assertTrue(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is not opened.
    assertFalse(page.$$('#safetyCheckCollapse').opened);

    // User starts check.
    page.$$('#safetyCheckParentButton').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.SAFETY_CHECK_START,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.Start',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    await safetyCheckBrowserProxy.whenCalled('runSafetyCheck');

    flush();
    // Only the icon button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertTrue(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(page.$$('#safetyCheckCollapse').opened);

    // Mock all incoming messages that indicate safety check completion.
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATED);
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.SAFE);
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD);
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS);
    fireSafetyCheckParentEvent(SafetyCheckParentStatus.AFTER);

    flush();
    // Only the icon button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertTrue(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(page.$$('#safetyCheckCollapse').opened);

    // Ensure the automatic browser proxy calls are started.
    return safetyCheckBrowserProxy.whenCalled('getParentRanDisplayString');
  });

  test('HappinessTrackingSurveysTest', function() {
    const testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.instance_ = testHatsBrowserProxy;
    page.$$('#safetyCheckParentButton').click();
    return testHatsBrowserProxy.whenCalled('tryShowSurvey');
  });

  test('updatesCheckingUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.CHECKING);
    flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
    assertIconStatusRunning(page.$$('#updatesIcon'));
  });

  test('updatesUpdatedUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATED);
    flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
    assertIconStatusSafe(page.$$('#updatesIcon'));
  });

  test('updatesUpdatingUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATING);
    flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
    assertIconStatusRunning(page.$$('#updatesIcon'));
  });

  test('updatesRelaunchUiTest', async function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.RELAUNCH);
    flush();
    assertTrue(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
    assertIconStatusInfo(page.$$('#updatesIcon'));

    // User clicks the relaunch button.
    page.$$('#safetyCheckUpdatesButton').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.SAFETY_CHECK_UPDATES_RELAUNCH,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.RelaunchAfterUpdates',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('updatesDisabledByAdminUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN);
    flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertTrue(!!page.$$('#safetyCheckUpdatesManagedIcon'));
    assertIconStatusInfo(page.$$('#updatesIcon'));
  });

  test('updatesFailedOfflineUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.FAILED_OFFLINE);
    flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
    assertIconStatusInfo(page.$$('#updatesIcon'));
  });

  test('updatesFailedUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.FAILED);
    flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
    assertIconStatusWarning(page.$$('#updatesIcon'));
  });

  test('updatesUnknownUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UNKNOWN);
    flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
    assertIconStatusInfo(page.$$('#updatesIcon'));
  });

  test('passwordsUiTest', function() {
    // Iterate over all states
    for (const state of Object.values(SafetyCheckPasswordsStatus)) {
      fireSafetyCheckPasswordsEvent(state);
      flush();

      // Button is only visible in COMPROMISED state
      assertEquals(
          state === SafetyCheckPasswordsStatus.COMPROMISED,
          !!page.$$('#safetyCheckPasswordsButton'));

      // Check that icon status is the correct one for this password status.
      switch (state) {
        case SafetyCheckPasswordsStatus.CHECKING:
          assertIconStatusRunning(page.$$('#passwordsIcon'));
          break;
        case SafetyCheckPasswordsStatus.SAFE:
          assertIconStatusSafe(page.$$('#passwordsIcon'));
          break;
        case SafetyCheckPasswordsStatus.COMPROMISED:
          assertIconStatusWarning(page.$$('#passwordsIcon'));
          break;
        case SafetyCheckPasswordsStatus.OFFLINE:
        case SafetyCheckPasswordsStatus.NO_PASSWORDS:
        case SafetyCheckPasswordsStatus.SIGNED_OUT:
        case SafetyCheckPasswordsStatus.QUOTA_LIMIT:
        case SafetyCheckPasswordsStatus.ERROR:
          assertIconStatusInfo(page.$$('#passwordsIcon'));
          break;
        default:
          assertNotReached();
      }
    }
  });

  test('passwordsCompromisedUiTest', async function() {
    loadTimeData.overrideValues({enablePasswordCheck: true});
    const passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.instance_ = passwordManager;

    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.COMPROMISED);
    flush();
    assertTrue(!!page.$$('#safetyCheckPasswordsButton'));
    assertIconStatusWarning(page.$$('#passwordsIcon'));

    // User clicks the manage passwords button.
    page.$$('#safetyCheckPasswordsButton').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.SAFETY_CHECK_PASSWORDS_MANAGE,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManagePasswords',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the correct Settings page is shown.
    assertEquals(
        routes.CHECK_PASSWORDS, Router.getInstance().getCurrentRoute());

    // Ensure correct referrer sent to password check.
    const referrer =
        await passwordManager.whenCalled('recordPasswordCheckReferrer');
    assertEquals(
        PasswordManagerProxy.PasswordCheckReferrer.SAFETY_CHECK, referrer);
  });

  test('safeBrowsingCheckingUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(SafetyCheckSafeBrowsingStatus.CHECKING);
    flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
    assertIconStatusRunning(page.$$('#safeBrowsingIcon'));
  });

  test('safeBrowsingEnabledStandardUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD);
    flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
    assertIconStatusSafe(page.$$('#safeBrowsingIcon'));
  });

  test('safeBrowsingEnabledEnhancedUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.ENABLED_ENHANCED);
    flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
    assertIconStatusSafe(page.$$('#safeBrowsingIcon'));
  });

  test('safeBrowsingDisabledUiTest', async function() {
    fireSafetyCheckSafeBrowsingEvent(SafetyCheckSafeBrowsingStatus.DISABLED);
    flush();
    assertTrue(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
    assertIconStatusInfo(page.$$('#safeBrowsingIcon'));

    // User clicks the manage safe browsing button.
    page.$$('#safetyCheckSafeBrowsingButton').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.SAFETY_CHECK_SAFE_BROWSING_MANAGE,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManageSafeBrowsing',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the correct Settings page is shown.
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });

  test('safeBrowsingDisabledByAdminUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN);
    flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertTrue(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
    assertIconStatusInfo(page.$$('#safeBrowsingIcon'));
  });

  test('safeBrowsingDisabledByExtensionUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION);
    flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertTrue(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
    assertIconStatusInfo(page.$$('#safeBrowsingIcon'));
  });

  test('extensionsCheckingUiTest', function() {
    fireSafetyCheckExtensionsEvent(SafetyCheckExtensionsStatus.CHECKING);
    flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
    assertIconStatusRunning(page.$$('#extensionsIcon'));
  });

  test('extensionsErrorUiTest', function() {
    fireSafetyCheckExtensionsEvent(SafetyCheckExtensionsStatus.ERROR);
    flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
    assertIconStatusInfo(page.$$('#extensionsIcon'));
  });

  test('extensionsSafeUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS);
    flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
    assertIconStatusSafe(page.$$('#extensionsIcon'));
  });

  test('extensionsBlocklistedOffUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED);
    flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
    assertIconStatusSafe(page.$$('#extensionsIcon'));

    return expectExtensionsButtonClickActions();
  });

  test('extensionsBlocklistedOnAllUserUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER);
    flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
    assertIconStatusWarning(page.$$('#extensionsIcon'));

    return expectExtensionsButtonClickActions();
  });

  test('extensionsBlocklistedOnUserAdminUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_SOME_BY_USER);
    flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
    assertIconStatusWarning(page.$$('#extensionsIcon'));

    return expectExtensionsButtonClickActions();
  });

  test('extensionsBlocklistedOnAllAdminUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN);
    flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertTrue(!!page.$$('#safetyCheckExtensionsManagedIcon'));
    assertIconStatusInfo(page.$$('#extensionsIcon'));
  });
});
