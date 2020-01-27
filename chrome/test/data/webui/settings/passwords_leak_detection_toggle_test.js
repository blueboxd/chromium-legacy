// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('CrSettingsPasswordsLeakDetectionToggleTest', function() {
  /** @type {settings.TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let privacyPageBrowserProxy;

  /** @type {settings.SyncBrowserProxy} */
  let syncBrowserProxy;

  /** @type {SettingsPersonalizationOptionsElement} */
  let testElement;

  setup(function() {
    privacyPageBrowserProxy = new TestPrivacyPageBrowserProxy();
    settings.PrivacyPageBrowserProxyImpl.instance_ = privacyPageBrowserProxy;
    syncBrowserProxy = new TestSyncBrowserProxy();
    settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    settings.MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    PolymerTest.clearBody();
    testElement =
        document.createElement('settings-passwords-leak-detection-toggle');
    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
    };
    document.body.appendChild(testElement);
    Polymer.dom.flush();
  });

  teardown(function() {
    testElement.remove();
  });

  test('logPasswordLeakDetectionClick', function() {
    testElement.set(
        'prefs.profile.password_manager_leak_detection.value', true);
    testElement.syncStatus = {signedIn: true};
    Polymer.dom.flush();
    console.log(testElement.$$('#passwordsLeakDetectionCheckbox').disabled);
    testElement.$$('#passwordsLeakDetectionCheckbox').click();
    return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
        .then(result => {
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_PASSWORD_CHECK, result);
        });
  });

  test('leakDetectionToggleSignedOutWithFalsePref', function() {
    testElement.set(
        'prefs.profile.password_manager_leak_detection.value', false);
    testElement.syncStatus = {signedIn: false};
    Polymer.dom.flush();

    assertTrue(testElement.$.passwordsLeakDetectionCheckbox.disabled);
    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
    assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
  });

  test('leakDetectionToggleSignedOutWithTruePref', function() {
    testElement.syncStatus = {signedIn: false};
    Polymer.dom.flush();

    assertTrue(testElement.$.passwordsLeakDetectionCheckbox.disabled);
    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
    assertEquals(
        loadTimeData.getString(
            'passwordsLeakDetectionSignedOutEnabledDescription'),
        testElement.$.passwordsLeakDetectionCheckbox.subLabel);
  });

  if (!cr.isChromeOS) {
    test('leakDetectionToggleSignedInNotSyncingWithFalsePref', function() {
      testElement.set(
          'prefs.profile.password_manager_leak_detection.value', false);
      testElement.syncStatus = {signedIn: false};
      sync_test_util.simulateStoredAccounts([
        {
          fullName: 'testName',
          givenName: 'test',
          email: 'test@test.com',
        },
      ]);
      Polymer.dom.flush();

      assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
      assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
    });

    test('leakDetectionToggleSignedInNotSyncingWithTruePref', function() {
      testElement.syncStatus = {signedIn: false};
      sync_test_util.simulateStoredAccounts([
        {
          fullName: 'testName',
          givenName: 'test',
          email: 'test@test.com',
        },
      ]);
      Polymer.dom.flush();

      assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
      assertTrue(testElement.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
    });
  }

  test('leakDetectionToggleSignedInAndSyncingWithFalsePref', function() {
    testElement.set(
        'prefs.profile.password_manager_leak_detection.value', false);
    testElement.syncStatus = {signedIn: true};
    Polymer.dom.flush();

    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
    assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
  });

  test('leakDetectionToggleSignedInAndSyncingWithTruePref', function() {
    testElement.syncStatus = {signedIn: true};
    Polymer.dom.flush();

    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
    assertTrue(testElement.$.passwordsLeakDetectionCheckbox.checked);
    assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
  });
});
