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

  test('parentBeforeCheckUiTest', function() {
    // Only the text button is present.
    assertTrue(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
  });

  test('parentDuringCheckUiTest', function() {
    // User starts check.
    page.$$('#safetyCheckParentButton').click();

    Polymer.dom.flush();
    // No button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
  });

  test('parentAfterCheckUiTest', function() {
    // User starts check.
    page.$$('#safetyCheckParentButton').click();

    // Mock all incoming messages that indicate safety check completion.
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: 0, /* UPDATES */
      newState: settings.SafetyCheckUpdatesStatus.UPDATED,
    });
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: 1, /* PASSWORDS */
      newState: settings.SafetyCheckPasswordsStatus.SAFE,
    });
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: 2, /* SAFE_BROWSING */
      newState: settings.SafetyCheckSafeBrowsingStatus.ENABLED,
    });
    cr.webUIListenerCallback('safety-check-status-changed', {
      safetyCheckComponent: 3, /* EXTENSIONS */
      newState: settings.SafetyCheckExtensionsStatus.SAFE,
    });

    Polymer.dom.flush();

    // Only the icon button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertTrue(!!page.$$('#safetyCheckParentIconButton'));
  });
});
