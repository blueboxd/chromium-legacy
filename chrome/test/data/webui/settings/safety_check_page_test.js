// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SafetyCheckUiTests', function() {
  /** @type {SettingsBasicPageElement} */
  let page;

  setup(function() {
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
    cr.webUIListenerCallback(
        settings.SafetyCheckCallbackConstants.UPDATES_CHANGED, event);
  }

  function fireSafetyCheckPasswordsEvent(state) {
    const event = {};
    event.newState = state;
    event.passwordsDisplayString = null;
    cr.webUIListenerCallback(
        settings.SafetyCheckCallbackConstants.PASSWORDS_CHANGED, event);
  }

  function fireSafetyCheckSafeBrowsingEvent(state) {
    const event = {};
    event.newState = state;
    cr.webUIListenerCallback(
        settings.SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED, event);
  }

  function fireSafetyCheckExtensionsEvent(state) {
    const event = {};
    event.newState = state;
    event.extensionsDisplayString = null;
    cr.webUIListenerCallback(
        settings.SafetyCheckCallbackConstants.EXTENSIONS_CHANGED, event);
  }

  /** Tests parent element and collapse. */
  test('beforeCheckUiTest', function() {
    // Only the text button is present.
    assertTrue(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is not opened.
    assertFalse(page.$$('#safetyCheckCollapse').opened);
  });

  /** Tests parent element and collapse. */
  test('duringCheckUiTest', function() {
    // User starts check.
    page.$$('#safetyCheckParentButton').click();

    Polymer.dom.flush();
    // No button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(page.$$('#safetyCheckCollapse').opened);
  });

  /** Tests parent element and collapse. */
  test('afterCheckUiTest', function() {
    // User starts check.
    page.$$('#safetyCheckParentButton').click();

    // Mock all incoming messages that indicate safety check completion.
    fireSafetyCheckUpdatesEvent(settings.SafetyCheckUpdatesStatus.UPDATED);
    fireSafetyCheckPasswordsEvent(settings.SafetyCheckPasswordsStatus.SAFE);
    fireSafetyCheckSafeBrowsingEvent(
        settings.SafetyCheckSafeBrowsingStatus.ENABLED);
    fireSafetyCheckExtensionsEvent(settings.SafetyCheckExtensionsStatus.SAFE);

    Polymer.dom.flush();

    // Only the icon button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertTrue(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(page.$$('#safetyCheckCollapse').opened);
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

      // button is only visible in COMPROMISED and ERROR states
      switch (state) {
        case settings.SafetyCheckPasswordsStatus.COMPROMISED:
        case settings.SafetyCheckPasswordsStatus.ERROR:
          assertTrue(!!page.$$('#safetyCheckPasswordsButton'));
          break;
        default:
          assertFalse(!!page.$$('#safetyCheckPasswordsButton'));
          break;
      }
    }
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

  test('extensionsSafeUiTest', function() {
    fireSafetyCheckExtensionsEvent(settings.SafetyCheckExtensionsStatus.SAFE);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsBadExtensionsOnUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_ON);
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsBadExtensionsOffUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_OFF);
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsManagedByAdminUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        settings.SafetyCheckExtensionsStatus.MANAGED_BY_ADMIN);
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertTrue(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });
});
