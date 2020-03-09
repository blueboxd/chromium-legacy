// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('CrSettingsSecurityPageTest', function() {
  /** @type {settings.TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {settings.SyncBrowserProxy} */
  let syncBrowserProxy;

  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testPrivacyBrowserProxy;

  /** @type {SettingsSecurityPageElement} */
  let page;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    settings.MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    settings.PrivacyPageBrowserProxyImpl.instance_ = testPrivacyBrowserProxy;
    syncBrowserProxy = new TestSyncBrowserProxy();
    settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-security-page');
    page.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
      },
      safebrowsing: {
        enabled: {value: true},
        scout_reporting_enabled: {value: true},
        enhanced: {value: false}
      },
    };
    document.body.appendChild(page);
    Polymer.dom.flush();
  });

  teardown(function() {
    page.remove();
  });

  if (cr.isMac || cr.isWindows) {
    test('NativeCertificateManager', function() {
      page.$$('#manageCertificates').click();
      return testPrivacyBrowserProxy.whenCalled('showManageSSLCertificates');
    });
  }

  test('safeBrowsingReportingToggle', function() {
    page.$$('#safeBrowsingStandard').click();
    const safeBrowsingReportingToggle = page.$.safeBrowsingReportingToggle;
    assertTrue(
        page.prefs.safebrowsing.enabled.value &&
        !page.prefs.safebrowsing.enhanced.value);
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
    // This could also be set to disabled, anything other than standard.
    page.$$('#safeBrowsingEnhanced').click();
    Polymer.dom.flush();

    assertFalse(
        page.prefs.safebrowsing.enabled.value &&
        !page.prefs.safebrowsing.enhanced.value);
    assertTrue(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
    assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);
    page.$$('#safeBrowsingStandard').click();
    Polymer.dom.flush();

    assertTrue(
        page.prefs.safebrowsing.enabled.value &&
        !page.prefs.safebrowsing.enhanced.value);
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
  });

  test('noControlSafeBrowsingReportingInEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    Polymer.dom.flush();

    assertFalse(page.$.safeBrowsingReportingToggle.disabled);
    page.$$('#safeBrowsingEnhanced').click();
    Polymer.dom.flush();

    assertTrue(page.$.safeBrowsingReportingToggle.disabled);
  });

  test('noValueChangeSafeBrowsingReportingInEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    Polymer.dom.flush();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$$('#safeBrowsingEnhanced').click();
    Polymer.dom.flush();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value == previous);
  });

  test('noControlSafeBrowsingReportingInDisabled', function() {
    page.$$('#safeBrowsingStandard').click();
    Polymer.dom.flush();

    assertFalse(page.$.safeBrowsingReportingToggle.disabled);
    page.$$('#safeBrowsingEnhanced').click();
    Polymer.dom.flush();

    assertTrue(page.$.safeBrowsingReportingToggle.disabled);
  });

  test('noValueChangeSafeBrowsingReportingInDisabled', function() {
    page.$$('#safeBrowsingStandard').click();
    Polymer.dom.flush();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$$('#safeBrowsingDisabled').click();
    Polymer.dom.flush();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value == previous);
  });

  test('noValueChangePasswordLeakSwitchToEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    Polymer.dom.flush();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$$('#safeBrowsingEnhanced').click();
    Polymer.dom.flush();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value == previous);
  });

  test('noValuePasswordLeakSwitchToDisabled', function() {
    page.$$('#safeBrowsingStandard').click();
    Polymer.dom.flush();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$$('#safeBrowsingDisabled').click();
    Polymer.dom.flush();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value == previous);
  });
});
