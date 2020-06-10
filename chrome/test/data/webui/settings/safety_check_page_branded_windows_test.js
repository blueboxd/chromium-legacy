// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
// clang-format on

suite('SafetyCheckPageChromeCleanerFlagDisabledTests', function() {
  /** @type {!SettingsSafetyCheckPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      safetyCheckChromeCleanerChildEnabled: false,
    });
  });

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsSafetyCheckPageElement} */ (
        document.createElement('settings-safety-check-page'));
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('testChromeCleanerNotPresent', async function() {
    // User starts check.
    page.$$('#safetyCheckParentButton').click();
    flush();
    // There is no Chrome cleaner child in safety check.
    assertFalse(!!page.$$('#chromeCleanerChild'));
  });
});

suite('SafetyCheckPageChromeCleanerFlagEnabledTests', function() {
  /** @type {!SettingsSafetyCheckPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      safetyCheckChromeCleanerChildEnabled: true,
    });
  });

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsSafetyCheckPageElement} */ (
        document.createElement('settings-safety-check-page'));
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('testChromeCleanerNotPresent', async function() {
    // User starts check.
    page.$$('#safetyCheckParentButton').click();
    flush();
    // There is a Chrome cleaner child in safety check.
    assertTrue(!!page.$$('#chromeCleanerChild'));
  });
});
