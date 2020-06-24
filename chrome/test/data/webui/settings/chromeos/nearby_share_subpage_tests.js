// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

suite('NearbyShare', function() {
  /** @type {?SettingsNearbyShareSubpage} */
  let subpage = null;
  /** @type {?HTMLElement} */
  let onOffText = null;
  /** @type {?SettingsToggleButtonElement} */
  let featureToggleButton = null;
  /** @type {?HTMLElement} */
  let toggleRow = null;

  setup(function() {
    PolymerTest.clearBody();
    subpage = document.createElement('settings-nearby-share-subpage');
    subpage.prefs = {
      'nearby_sharing': {
        'enabled': {
          value: true,
        }
      }
    };

    document.body.appendChild(subpage);
    Polymer.dom.flush();

    onOffText = subpage.$$('#onOff');
    featureToggleButton = subpage.$$('#featureToggleButton');
    toggleRow = subpage.$$('#toggleRow');
  });

  teardown(function() {
    subpage.remove();
  });

  test('feature toggle button controls preference', function() {
    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', onOffText.textContent.trim());

    featureToggleButton.click();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', onOffText.textContent.trim());
  });

  test('toggle row controls preference', function() {
    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', onOffText.textContent.trim());

    toggleRow.click();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', onOffText.textContent.trim());
  });
});
