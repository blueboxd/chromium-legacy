// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {SafeBrowsingBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {PrivacyPageBrowserProxyImpl, SyncBrowserProxyImpl, MetricsBrowserProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';
// #import {TestMetricsBrowserProxy} from 'chrome://test/settings/test_metrics_browser_proxy.m.js';
// #import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';
// #import {TestPrivacyPageBrowserProxy} from 'chrome://test/settings/test_privacy_page_browser_proxy.m.js';
// #import {TestSafeBrowsingBrowserProxy} from 'chrome://test/settings/test_safe_browsing_browser_proxy.m.js';
// #import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
// #import {isMac, isWindows} from 'chrome://resources/js/cr.m.js';
// clang-format on

suite('CrSettingsSecurityPageTest', function() {
  /** @type {settings.TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {settings.SyncBrowserProxy} */
  let syncBrowserProxy;

  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testPrivacyBrowserProxy;

  /** @type {settings.SafeBrowsingBrowserProxy} */
  let testSafeBrowsingBrowserProxy;

  /** @type {SettingsSecurityPageElement} */
  let page;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    settings.MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    settings.PrivacyPageBrowserProxyImpl.instance_ = testPrivacyBrowserProxy;
    syncBrowserProxy = new TestSyncBrowserProxy();
    settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
    testSafeBrowsingBrowserProxy = new TestSafeBrowsingBrowserProxy();
    settings.SafeBrowsingBrowserProxyImpl.instance_ =
        testSafeBrowsingBrowserProxy;
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

  test('LogManageCerfificatesClick', function() {
    page.$$('#manageCertificates').click();
    return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
        .then(result => {
          assertEquals(
              settings.PrivacyElementInteractions.MANAGE_CERTIFICATES, result);
        });
  });

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

  test('SafeBrowsingRadio_ManagedState', async function() {
    const enhancedRadio = page.$$('#safeBrowsingEnhanced');
    const standardRadio = page.$$('#safeBrowsingStandard');
    const disabledRadio = page.$$('#safeBrowsingDisabled');

    const managedRadioState = {
      enhanced:
          {disabled: true, indicator: CrPolicyIndicatorType.DEVICE_POLICY},
      standard:
          {disabled: true, indicator: CrPolicyIndicatorType.DEVICE_POLICY},
      disabled:
          {disabled: true, indicator: CrPolicyIndicatorType.DEVICE_POLICY},
    };

    // Make sure the element has requested the managed state from its init.
    await testSafeBrowsingBrowserProxy.whenCalled(
        'getSafeBrowsingRadioManagedState');
    testSafeBrowsingBrowserProxy.reset();

    testSafeBrowsingBrowserProxy.setResultFor(
        'getSafeBrowsingRadioManagedState', Promise.resolve(managedRadioState));
    // Change an arbitrary Safe Browsing pref to trigger managed state update.
    page.set('prefs.safebrowsing.enabled.value', false);
    await testSafeBrowsingBrowserProxy.whenCalled(
        'getSafeBrowsingRadioManagedState');
    testSafeBrowsingBrowserProxy.reset();
    Polymer.dom.flush();

    assertTrue(enhancedRadio.disabled);
    assertEquals(
        enhancedRadio.policyIndicatorType, CrPolicyIndicatorType.DEVICE_POLICY);
    assertTrue(standardRadio.disabled);
    assertEquals(
        standardRadio.policyIndicatorType, CrPolicyIndicatorType.DEVICE_POLICY);
    assertTrue(disabledRadio.disabled);
    assertEquals(
        disabledRadio.policyIndicatorType, CrPolicyIndicatorType.DEVICE_POLICY);

    // Ensure reverting to an unmanaged state is correctly handled.
    const unmanagedRadioState = {
      enhanced: {disabled: false, indicator: CrPolicyIndicatorType.NONE},
      standard: {disabled: false, indicator: CrPolicyIndicatorType.NONE},
      disabled: {disabled: false, indicator: CrPolicyIndicatorType.NONE},
    };
    testSafeBrowsingBrowserProxy.setResultFor(
        'getSafeBrowsingRadioManagedState',
        Promise.resolve(unmanagedRadioState));
    // Change an arbitrary Safe Browsing pref to trigger managed state update.
    page.set('prefs.safebrowsing.enabled.value', true);
    await testSafeBrowsingBrowserProxy.whenCalled(
        'getSafeBrowsingRadioManagedState');
    testSafeBrowsingBrowserProxy.reset();
    Polymer.dom.flush();

    assertFalse(enhancedRadio.disabled);
    assertEquals(enhancedRadio.policyIndicatorType, CrPolicyIndicatorType.NONE);
    assertFalse(standardRadio.disabled);
    assertEquals(standardRadio.policyIndicatorType, CrPolicyIndicatorType.NONE);
    assertFalse(disabledRadio.disabled);
    assertEquals(disabledRadio.policyIndicatorType, CrPolicyIndicatorType.NONE);
  });
});
