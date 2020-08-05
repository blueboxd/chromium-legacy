// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// clang-format on

suite('PrivacyPageTests', function() {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    privacyPage.remove();
  });

  test('Suggested content, visibility disabled', async () => {
    loadTimeData.overrideValues({
      suggestedContentToggleEnabled: false,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    assertEquals(null, privacyPage.$$('#suggested-content'));
  });

  test('Suggested content, visibility enabled', async () => {
    loadTimeData.overrideValues({
      suggestedContentToggleEnabled: true,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    Polymer.dom.flush();

    // The default state of the pref is disabled.
    const suggestedContent = assert(privacyPage.$$('#suggested-content'));
    assertFalse(suggestedContent.checked);
  });

  test('Suggested content, pref enabled', async () => {
    loadTimeData.overrideValues({
      suggestedContentToggleEnabled: true,
    });

    privacyPage = document.createElement('os-settings-privacy-page');

    // Update the backing pref to enabled.
    privacyPage.prefs = {
      'settings': {
        'suggested_content_enabled': {
          value: true,
        }
      }
    };
    document.body.appendChild(privacyPage);

    Polymer.dom.flush();

    // The checkbox reflects the updated pref state.
    const suggestedContent = assert(privacyPage.$$('#suggested-content'));
    assertTrue(suggestedContent.checked);
  });
});
