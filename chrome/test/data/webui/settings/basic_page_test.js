// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings basic page. */

// clang-format off
import 'chrome://settings/settings.js';

import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {pageVisibility} from 'chrome://settings/settings.js';
// clang-format on

// Register mocha tests.
suite('SettingsBasicPage', function() {
  let page = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: true,
    });
  });

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
  });

  test('load page', function() {
    // This will fail if there are any asserts or errors in the Settings page.
  });

  test('basic pages', function() {
    const sections = [
      'appearance', 'onStartup', 'people', 'search', 'autofill', 'safetyCheck',
      'privacy'
    ];
    if (!isChromeOS) {
      sections.push('defaultBrowser');
    }

    flush();

    for (const section of sections) {
      const sectionElement = page.$$(`settings-section[section=${section}]`);
      assertTrue(!!sectionElement);
    }
  });

  test('safetyCheckVisibilityTest', function() {
    // Set the visibility of the pages under test to "false".
    page.pageVisibility = Object.assign(pageVisibility || {}, {
      safetyCheck: false,
    });
    flush();

    const sectionElement = page.$$('settings-section-safety-check');
    assertFalse(!!sectionElement);
  });
});

suite('SettingsBasicPagePrivacyRedesignFlagOff', function() {
  let page = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: false,
    });
  });

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
  });

  test('load page', function() {
    // This will fail if there are any asserts or errors in the Settings page.
  });

  test('safetyCheckNotInBasicPage', function() {
    flush();
    assertFalse(!!page.$$('settings-section[section=safetyCheck'));
  });
});
