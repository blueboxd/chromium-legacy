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
    assertTrue(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
  });

  test('parentDuringCheckUiTest', function() {
    page.$$('#safetyCheckParentButton').click();

    Polymer.dom.flush();

    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
  });
});
