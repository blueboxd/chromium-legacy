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
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.UPDATES,
      newState: settings.SafetyCheckUpdatesStatus.UPDATED,
    });
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.PASSWORDS,
      newState: settings.SafetyCheckPasswordsStatus.SAFE,
    });
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.SAFE_BROWSING,
      newState: settings.SafetyCheckSafeBrowsingStatus.ENABLED,
    });
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.EXTENSIONS,
      newState: settings.SafetyCheckExtensionsStatus.SAFE,
    });

    Polymer.dom.flush();

    // Only the icon button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertTrue(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(page.$$('#safetyCheckCollapse').opened);
  });

  test('updatesCheckingUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.UPDATES,
      newState: settings.SafetyCheckUpdatesStatus.CHECKING,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesUpdatedUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.UPDATES,
      newState: settings.SafetyCheckUpdatesStatus.UPDATED,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesUpdatingUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.UPDATES,
      newState: settings.SafetyCheckUpdatesStatus.UPDATING,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesRelaunchUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.UPDATES,
      newState: settings.SafetyCheckUpdatesStatus.RELAUNCH,
    });
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesDisabledByAdminUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.UPDATES,
      newState: settings.SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertTrue(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesFailedOfflineUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.UPDATES,
      newState: settings.SafetyCheckUpdatesStatus.FAILED_OFFLINE,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('updatesFailedUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.UPDATES,
      newState: settings.SafetyCheckUpdatesStatus.FAILED,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckUpdatesButton'));
    assertFalse(!!page.$$('#safetyCheckUpdatesManagedIcon'));
  });

  test('passwordsButtonVisibilityUiTest', function() {
    // Iterate over all states
    for (const state of Object.values(settings.SafetyCheckPasswordsStatus)) {
      cr.webUIListenerCallback('safety-check-status-changed', {
        safetyCheckComponent: settings.SafetyCheckComponent.PASSWORDS,
        newState: state,
      });
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

  test('extensionsCheckingUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.EXTENSIONS,
      newState: settings.SafetyCheckExtensionsStatus.CHECKING,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsErrorUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.EXTENSIONS,
      newState: settings.SafetyCheckExtensionsStatus.ERROR,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsSafeUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.EXTENSIONS,
      newState: settings.SafetyCheckExtensionsStatus.SAFE,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsBadExtensionsOnUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.EXTENSIONS,
      newState: settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_ON,
    });
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsBadExtensionsOffUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.EXTENSIONS,
      newState: settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_OFF,
    });
    Polymer.dom.flush();
    assertTrue(!!page.$$('#safetyCheckExtensionsButton'));
    assertFalse(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });

  test('extensionsManagedByAdminUiTest', function() {
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: settings.SafetyCheckComponent.EXTENSIONS,
      newState: settings.SafetyCheckExtensionsStatus.MANAGED_BY_ADMIN,
    });
    Polymer.dom.flush();
    assertFalse(!!page.$$('#safetyCheckExtensionsButton'));
    assertTrue(!!page.$$('#safetyCheckExtensionsManagedIcon'));
  });
});
