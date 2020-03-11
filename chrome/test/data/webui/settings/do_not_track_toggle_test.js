// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('CrSettingsDoNotTrackToggleTest', function() {
  /** @type {settings.TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {SettingsDoNotTrackToggle} */
  let testElement;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    settings.MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('settings-do-not-track-toggle');
    testElement.prefs = {
      enable_do_not_track: {value: false},
    };
    document.body.appendChild(testElement);
    Polymer.dom.flush();
  });

  teardown(function() {
    testElement.remove();
  });

  test('logDoNotTrackClick', async function() {
    testElement.$.toggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(settings.PrivacyElementInteractions.DO_NOT_TRACK, result);
  });

  test('DialogAndToggleBehavior', function() {
    testElement.$.toggle.click();
    Polymer.dom.flush();
    assertTrue(testElement.$.toggle.checked);

    testElement.$$('.cancel-button').click();
    assertFalse(testElement.$.toggle.checked);
    assertFalse(testElement.prefs.enable_do_not_track.value);

    testElement.$.toggle.click();
    Polymer.dom.flush();
    assertTrue(testElement.$.toggle.checked);
    testElement.$$('.action-button').click();
    assertTrue(testElement.$.toggle.checked);
    assertTrue(testElement.prefs.enable_do_not_track.value);
  });
});
