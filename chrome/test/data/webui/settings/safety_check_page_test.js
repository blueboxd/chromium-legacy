// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {HatsBrowserProxyImpl, LifetimeBrowserProxyImpl, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerProxy, Router, routes, SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckExtensionsStatus, SafetyCheckPasswordsStatus, SafetyCheckSafeBrowsingStatus, SafetyCheckUpdatesStatus} from 'chrome://settings/settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// #import {TestHatsBrowserProxy} from 'chrome://test/settings/test_hats_browser_proxy.m.js';
// #import {TestLifetimeBrowserProxy} from 'chrome://test/settings/test_lifetime_browser_proxy.m.js';
// #import {TestPasswordManagerProxy} from 'chrome://test/settings/test_password_manager_proxy.m.js';
// #import {TestOpenWindowProxy} from 'chrome://test/settings/test_open_window_proxy.m.js';
// clang-format on

suite('SafetyCheckUiTests', function() {
  /** @type {?settings.LifetimeBrowserProxy} */
  let lifetimeBrowserProxy = null;
  /** @type {settings.OpenWindowProxy} */
  let openWindowProxy = null;
  /** @type {settings.SafetyCheckBrowserProxy} */
  let safetyCheckBrowserProxy = null;
  /** @type {SettingsBasicPageElement} */
  let page;

  setup(function() {
    lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
    settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;
    openWindowProxy = new TestOpenWindowProxy();
    settings.OpenWindowProxyImpl.instance_ = openWindowProxy;
    safetyCheckBrowserProxy =
        TestBrowserProxy.fromClass(settings.SafetyCheckBrowserProxy);
    safetyCheckBrowserProxy.setResultFor(
        'getParentRanDisplayString', Promise.resolve('Dummy string'));
    settings.SafetyCheckBrowserProxyImpl.instance_ = safetyCheckBrowserProxy;

    PolymerTest.clearBody();
    page = document.createElement('settings-safety-check-page');
    document.body.appendChild(page);
    Polymer.dom.flush();
  });

  teardown(function() {
    page.remove();
  });

  function fireSafetyCheckUpdatesEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    cr.webUIListenerCallback(
        settings.SafetyCheckCallbackConstants.UPDATES_CHANGED, event);
  }

  function fireSafetyCheckPasswordsEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    event.passwordsButtonString = null;
    cr.webUIListenerCallback(
        settings.SafetyCheckCallbackConstants.PASSWORDS_CHANGED, event);
  }

  function fireSafetyCheckSafeBrowsingEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    cr.webUIListenerCallback(
        settings.SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED, event);
  }

  function fireSafetyCheckExtensionsEvent(state) {
    const event = {};
    event.newState = state;
    event.displayString = null;
    cr.webUIListenerCallback(
        settings.SafetyCheckCallbackConstants.EXTENSIONS_CHANGED, event);
  }
  /**
   * @return {!Promise}
   */
  async function expectExtensionsButtonClickActions() {
    // User clicks review extensions button.
    page.$$('#safetyCheckExtensionsButton').click();
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
    // Ensure the browser proxy call is done.
    await safetyCheckBrowserProxy.whenCalled('runSafetyCheck');

    Polymer.dom.flush();
    // No button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(page.$$('#safetyCheckCollapse').opened);

    // Mock all incoming messages that indicate safety check completion.
    fireSafetyCheckUpdatesEvent(settings.SafetyCheckUpdatesStatus.UPDATED);
    fireSafetyCheckPasswordsEvent(settings.SafetyCheckPasswordsStatus.SAFE);
    fireSafetyCheckSafeBrowsingEvent(
        settings.SafetyCheckSafeBrowsingStatus.ENABLED);
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS);

    Polymer.dom.flush();
    // Only the icon button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertTrue(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(page.$$('#safetyCheckCollapse').opened);

    // Ensure the automatic browser proxy calls are started.
    const timestamp =
        await safetyCheckBrowserProxy.whenCalled('getParentRanDisplayString');
    assertTrue(Number.isInteger(timestamp));
    assertTrue(timestamp >= 0);
  });

  test('HappinessTrackingSurveysTest', function() {
    const testHatsBrowserProxy = new TestHatsBrowserProxy();
    settings.HatsBrowserProxyImpl.instance_ = testHatsBrowserProxy;
    page.$$('#safetyCheckParentButton').click();
    return testHatsBrowserProxy.whenCalled('tryShowSurvey');
  });

  test('updatesCheckingUiTest', function() {
    fireSafetyCheckUpdatesEvent(settings.SafetyCheckUpdatesStatus.CHECKING);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesUpdatedUiTest', function() {
    fireSafetyCheckUpdatesEvent(settings.SafetyCheckUpdatesStatus.UPDATED);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesUpdatingUiTest', function() {
    fireSafetyCheckUpdatesEvent(settings.SafetyCheckUpdatesStatus.UPDATING);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesRelaunchUiTest', function() {
    fireSafetyCheckUpdatesEvent(settings.SafetyCheckUpdatesStatus.RELAUNCH);
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));

    // User clicks the relaunch button.
    page.$$('#safetyCheckUpdatesButton').click();
    // Ensure the browser proxy call is done.
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('updatesDisabledByAdminUiTest', function() {
    fireSafetyCheckUpdatesEvent(
        settings.SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertTrue(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesFailedOfflineUiTest', function() {
    fireSafetyCheckUpdatesEvent(
        settings.SafetyCheckUpdatesStatus.FAILED_OFFLINE);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesFailedUiTest', function() {
    fireSafetyCheckUpdatesEvent(settings.SafetyCheckUpdatesStatus.FAILED);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('passwordsButtonVisibilityUiTest', function() {
    // Iterate over all states
    for (const state of Object.values(settings.SafetyCheckPasswordsStatus)) {
      fireSafetyCheckPasswordsEvent(state);
      Polymer.dom.flush();

      // Button is only visible in COMPROMISED state
      assertEquals(
          state === settings.SafetyCheckPasswordsStatus.COMPROMISED,
          !!page.$$('#safetyCheckPasswordsButton'));
    }
  });

  test('passwordsCompromisedUiTest', async function() {
    loadTimeData.overrideValues({enablePasswordCheck: true});
    const passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.instance_ = passwordManager;

    fireSafetyCheckPasswordsEvent(
        settings.SafetyCheckPasswordsStatus.COMPROMISED);
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckPasswordsButton'));

    // User clicks the manage passwords button.
    page.$$('#safetyCheckPasswordsButton').click();

    // Ensure the correct Settings page is shown.
    assertEquals(
        settings.routes.CHECK_PASSWORDS,
        settings.Router.getInstance().getCurrentRoute());

    // Ensure correct referrer sent to password check.
    const referrer =
        await passwordManager.whenCalled('recordPasswordCheckReferrer');
    assertEquals(
        PasswordManagerProxy.PasswordCheckReferrer.SAFETY_CHECK, referrer);
  });

  test('safeBrowsingCheckingUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        settings.SafetyCheckSafeBrowsingStatus.CHECKING);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
  });

  test('safeBrowsingCheckingUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        settings.SafetyCheckSafeBrowsingStatus.ENABLED);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
  });

  test('safeBrowsingCheckingUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        settings.SafetyCheckSafeBrowsingStatus.DISABLED);
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));

    // User clicks the manage safe browsing button.
    page.$$('#safetyCheckSafeBrowsingButton').click();

    // Ensure the correct Settings page is shown.
    assertEquals(
        settings.routes.SECURITY,
        settings.Router.getInstance().getCurrentRoute());
  });

  test('safeBrowsingCheckingUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        settings.SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertTrue(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
  });

  test('safeBrowsingCheckingUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        settings.SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckSafeBrowsingButton'));
    assertTrue(!!page.$$('#safetyCheckSafeBrowsingManagedIcon'));
  });

  test('extensionsCheckingUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.CHECKING);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsErrorUiTest', function() {
    fireSafetyCheckExtensionsEvent(settings.SafetyCheckExtensionsStatus.ERROR);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });


  test('extensionsCheckingUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.CHECKING);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsErrorUiTest', function() {
    fireSafetyCheckExtensionsEvent(settings.SafetyCheckExtensionsStatus.ERROR);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsSafeUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsBlocklistedOffUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED);
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));

    return expectExtensionsButtonClickActions();
  });

  test('extensionsBlocklistedOnAllUserUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER);
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));

    return expectExtensionsButtonClickActions();
  });

  test('extensionsBlocklistedOnUserAdminUiTest', function() {
    fireSafetyCheckExtensionsEvent(settings.SafetyCheckExtensionsStatus
                                       .BLOCKLISTED_REENABLED_SOME_BY_USER);
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));

    return expectExtensionsButtonClickActions();
  });

  test('extensionsBlocklistedOnAllAdminUiTest', function() {
    fireSafetyCheckExtensionsEvent(settings.SafetyCheckExtensionsStatus
                                       .BLOCKLISTED_REENABLED_ALL_BY_ADMIN);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertTrue(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });
});
