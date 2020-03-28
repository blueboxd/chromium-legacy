// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';
// #import {TestMetricsBrowserProxy} from 'chrome://test/settings/test_metrics_browser_proxy.m.js';
// #import {PrivacyPageBrowserProxyImpl, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
// #import {TestPrivacyPageBrowserProxy} from 'chrome://test/settings/test_privacy_page_browser_proxy.m.js';
// #import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// #import {simulateStoredAccounts} from 'chrome://test/settings/sync_test_util.m.js';
// clang-format on

suite('CrSettingsPasswordsLeakDetectionToggleTest', function() {
  /** @type {settings.TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let privacyPageBrowserProxy;

  /** @type {settings.SyncBrowserProxy} */
  let syncBrowserProxy;

  /** @type {SettingsPersonalizationOptionsElement} */
  let testElement;

  /** @type {String} */
  let signedInSubLabel;

  /** @type {String} */
  let signedOutSubLabel;

  suiteSetup(function() {
    signedInSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription');
    signedOutSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription') +
        ' ' +
        loadTimeData.getString(
            'passwordsLeakDetectionSignedOutEnabledDescription') +
        loadTimeData.getString('sentenceEnd');
  });

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
      safebrowsing: {
        enabled: {value: true},
        scout_reporting_enabled: {value: true},
        enhanced: {value: false}
      },
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
              settings.PrivacyElementInteractions.PASSWORD_CHECK, result);
        });
  });

  test('leakDetectionToggleSignedOutWithFalsePref', function() {
    testElement.set(
        'prefs.profile.password_manager_leak_detection.value', false);
    testElement.syncStatus = {signedIn: false};
    Polymer.dom.flush();

    assertTrue(testElement.$.passwordsLeakDetectionCheckbox.disabled);
    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
    assertEquals(
        signedInSubLabel,
        testElement.$.passwordsLeakDetectionCheckbox.subLabel);
  });

  test('leakDetectionToggleSignedOutWithTruePref', function() {
    testElement.syncStatus = {signedIn: false};
    Polymer.dom.flush();

    assertTrue(testElement.$.passwordsLeakDetectionCheckbox.disabled);
    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
    assertEquals(
        signedOutSubLabel,
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
      assertEquals(
          signedInSubLabel,
          testElement.$.passwordsLeakDetectionCheckbox.subLabel);
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
      assertEquals(
          signedInSubLabel,
          testElement.$.passwordsLeakDetectionCheckbox.subLabel);
    });
  }

  test('leakDetectionToggleSignedInAndSyncingWithFalsePref', function() {
    testElement.set(
        'prefs.profile.password_manager_leak_detection.value', false);
    testElement.syncStatus = {signedIn: true};
    Polymer.dom.flush();

    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
    assertEquals(
        signedInSubLabel,
        testElement.$.passwordsLeakDetectionCheckbox.subLabel);
  });

  test('leakDetectionToggleSignedInAndSyncingWithTruePref', function() {
    testElement.syncStatus = {signedIn: true};
    Polymer.dom.flush();

    assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
    assertTrue(testElement.$.passwordsLeakDetectionCheckbox.checked);
    assertEquals(
        signedInSubLabel,
        testElement.$.passwordsLeakDetectionCheckbox.subLabel);
  });
});
