// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('CrSettingsSecurityPageTest', function() {
  /** @type {settings.SyncBrowserProxy} */
  let syncBrowserProxy;

  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testPrivacyBrowserProxy;

  /** @type {SettingsSecurityPageElement} */
  let page;

  setup(function() {
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
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
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
    return testPrivacyBrowserProxy.whenCalled('recordSettingsPageHistogram')
        .then(result => {
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_MANAGE_CERTIFICATES,
              result);
        });
  });

  test('safeBrowsingReportingToggle', function() {
    const safeBrowsingToggle = page.$.safeBrowsingToggle;
    const safeBrowsingReportingToggle = page.$.safeBrowsingReportingToggle;
    assertTrue(safeBrowsingToggle.checked);
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
    safeBrowsingToggle.click();
    Polymer.dom.flush();

    assertFalse(safeBrowsingToggle.checked);
    assertTrue(safeBrowsingReportingToggle.disabled);
    assertFalse(safeBrowsingReportingToggle.checked);
    assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);
    safeBrowsingToggle.click();
    Polymer.dom.flush();

    assertTrue(safeBrowsingToggle.checked);
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
  });
});
