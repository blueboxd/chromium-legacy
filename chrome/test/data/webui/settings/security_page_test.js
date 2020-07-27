// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SafeBrowsingSetting} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions,PrivacyPageBrowserProxyImpl, Router, routes, SecureDnsMode, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {flushTasks} from '../test_util.m.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.m.js';

// clang-format on

suite('CrSettingsSecurityPageTestWithEnhanced', function() {
  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {!TestSyncBrowserProxy} */
  let syncBrowserProxy;

  /** @type {!TestPrivacyPageBrowserProxy} */
  let testPrivacyBrowserProxy;

  /** @type {!SettingsSecurityPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: true,
      safeBrowsingEnhancedEnabled: true,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.instance_ = testPrivacyBrowserProxy;
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
    document.body.innerHTML = '';
    page = /** @type {!SettingsSecurityPageElement} */ (
        document.createElement('settings-security-page'));
    page.prefs = {
      profile: {password_manager_leak_detection: {value: false}},
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
      },
      safebrowsing: {
        enabled: {value: true},
        scout_reporting_enabled: {value: true},
        enhanced: {value: false}
      },
      generated: {
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
      },
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  if (isMac || isWindows) {
    test('NativeCertificateManager', function() {
      page.$$('#manageCertificates').click();
      return testPrivacyBrowserProxy.whenCalled('showManageSSLCertificates');
    });
  }

  test('LogManageCerfificatesClick', async function() {
    page.$$('#manageCertificates').click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.MANAGE_CERTIFICATES, result);
  });

  test('ManageSecurityKeysSubpageRoute', function() {
    page.$$('#security-keys-subpage-trigger').click();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.SECURITY_KEYS);
  });

  test('LogSafeBrowsingExtendedToggle', async function() {
    page.$$('#safeBrowsingStandard').click();
    flush();

    page.$$('#safeBrowsingReportingToggle').click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.IMPROVE_SECURITY, result);
  });

  test('safeBrowsingReportingToggle', function() {
    page.$$('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    const safeBrowsingReportingToggle = page.$$('#safeBrowsingReportingToggle');
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);

    // This could also be set to disabled, anything other than standard.
    page.$$('#safeBrowsingEnhanced').click();
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
    flush();
    assertTrue(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
    assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);

    page.$$('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
  });

  test('DisableSafebrowsingDialog_Confirm', async function() {
    page.$$('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();

    page.$$('#safeBrowsingDisabled').click();
    flush();

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(null, page.$$('settings-disable-safebrowsing-dialog'));

    assertFalse(page.$$('#safeBrowsingEnhanced').checked);
    assertFalse(page.$$('#safeBrowsingStandard').checked);
    assertTrue(page.$$('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.DISABLED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromEnhanced', async function() {
    page.$$('#safeBrowsingEnhanced').click();
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
    flush();

    page.$$('#safeBrowsingDisabled').click();
    flush();

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.cancel-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(null, page.$$('settings-disable-safebrowsing-dialog'));

    assertTrue(page.$$('#safeBrowsingEnhanced').checked);
    assertFalse(page.$$('#safeBrowsingStandard').checked);
    assertFalse(page.$$('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromStandard', async function() {
    page.$$('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();

    page.$$('#safeBrowsingDisabled').click();
    flush();

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.cancel-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(null, page.$$('settings-disable-safebrowsing-dialog'));

    assertFalse(page.$$('#safeBrowsingEnhanced').checked);
    assertTrue(page.$$('#safeBrowsingStandard').checked);
    assertFalse(page.$$('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
  });

  test('noControlSafeBrowsingReportingInEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    flush();

    assertFalse(page.$$('#safeBrowsingReportingToggle').disabled);
    page.$$('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(page.$$('#safeBrowsingReportingToggle').disabled);
  });

  test('noValueChangeSafeBrowsingReportingInEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$$('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noControlSafeBrowsingReportingInDisabled', async function() {
    page.$$('#safeBrowsingStandard').click();
    flush();

    assertFalse(page.$$('#safeBrowsingReportingToggle').disabled);
    page.$$('#safeBrowsingDisabled').click();
    flush();

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(page.$$('#safeBrowsingReportingToggle').disabled);
  });

  test('noValueChangeSafeBrowsingReportingInDisabled', async function() {
    page.$$('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$$('#safeBrowsingDisabled').click();
    flush();

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noValueChangePasswordLeakSwitchToEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$$('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });

  test('noValuePasswordLeakSwitchToDisabled', async function() {
    page.$$('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$$('#safeBrowsingDisabled').click();
    flush();

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });
});


suite('CrSettingsSecurityPageTestWithoutEnhanced', function() {
  /** @type {!SettingsSecurityPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      safeBrowsingEnhancedEnabled: false,
    });
  });

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsSecurityPageElement} */ (
        document.createElement('settings-security-page'));
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
      generated: {
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
      },
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('enhancedHiddenWhenDisbled', function() {
    assertTrue(page.$$('#safeBrowsingEnhanced').hidden);
  });
});
