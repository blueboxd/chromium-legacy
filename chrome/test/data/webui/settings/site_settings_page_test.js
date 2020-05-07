// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting,defaultSettingLabel,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

// clang-format on

suite('SiteSettingsPage', function() {
  /** @type {TestSiteSettingsPrefsBrowserProxy} */
  let siteSettingsBrowserProxy = null;

  /** @type {SettingsSiteSettingsPageElement} */
  let page;

  /** @type {Array<string>} */
  const testLabels = ['test label 1', 'test label 2'];

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: false,
    });
  });

  function setupPage() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = siteSettingsBrowserProxy;
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]);
    PolymerTest.clearBody();
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    flush();
  }

  setup(setupPage);

  teardown(function() {
    page.remove();
  });

  test('DefaultLabels', function() {
    assertEquals('a', defaultSettingLabel(ContentSetting.ALLOW, 'a', 'b'));
    assertEquals('b', defaultSettingLabel(ContentSetting.BLOCK, 'a', 'b'));
    assertEquals('a', defaultSettingLabel(ContentSetting.ALLOW, 'a', 'b', 'c'));
    assertEquals('b', defaultSettingLabel(ContentSetting.BLOCK, 'a', 'b', 'c'));
    assertEquals(
        'c', defaultSettingLabel(ContentSetting.SESSION_ONLY, 'a', 'b', 'c'));
    assertEquals(
        'c', defaultSettingLabel(ContentSetting.DEFAULT, 'a', 'b', 'c'));
    assertEquals('c', defaultSettingLabel(ContentSetting.ASK, 'a', 'b', 'c'));
    assertEquals(
        'c',
        defaultSettingLabel(ContentSetting.IMPORTANT_CONTENT, 'a', 'b', 'c'));
  });

  test('CookiesLinkRowSublabel', async function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: false,
    });
    setupPage();
    const allSettingsList = page.$$('#allSettingsList');
    await eventToPromise(
        'site-settings-list-labels-updated-for-testing', allSettingsList);
    assertEquals(
        allSettingsList.i18n('siteSettingsCookiesAllowed'),
        allSettingsList.$$('#cookies').subLabel);
  });

  test('CookiesLinkRowSublabel_Redesign', async function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: true,
    });
    setupPage();
    await siteSettingsBrowserProxy.whenCalled('getCookieSettingDescription');
    flush();
    const cookiesLinkRow = page.$$('#basicContentList').$$('#cookies');
    assertEquals(testLabels[0], cookiesLinkRow.subLabel);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(testLabels[1], cookiesLinkRow.subLabel);
  });
});
