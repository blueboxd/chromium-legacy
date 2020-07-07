// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, defaultSettingLabel, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise, isChildVisible} from '../test_util.m.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('SiteSettingsPage', function() {
  /** @type {?TestSiteSettingsPrefsBrowserProxy} */
  let siteSettingsBrowserProxy = null;

  /** @type {SettingsSiteSettingsPageElement} */
  let page;

  /** @type {Array<string>} */
  const testLabels = ['test label 1', 'test label 2'];

  function setupPage() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = siteSettingsBrowserProxy;
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]);
    document.body.innerHTML = '';
    page = /** @type {!SettingsSiteSettingsPageElement} */ (
        document.createElement('settings-site-settings-page'));
    document.body.appendChild(page);
    flush();
  }

  setup(setupPage);

  teardown(function() {
    page.remove();
  });

  test('DefaultLabels', function() {
    assertEquals(
        'a', defaultSettingLabel(ContentSetting.ALLOW, 'a', 'b', null));
    assertEquals(
        'b', defaultSettingLabel(ContentSetting.BLOCK, 'a', 'b', null));
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
    setupPage();
    await siteSettingsBrowserProxy.whenCalled('getCookieSettingDescription');
    flush();
    const cookiesLinkRow = /** @type {!CrLinkRowElement} */ (
        page.$$('#basicContentList').$$('#cookies'));
    assertEquals(testLabels[0], cookiesLinkRow.subLabel);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(testLabels[1], cookiesLinkRow.subLabel);
  });

  test('ProtectedContentRow', function() {
    setupPage();
    page.$$('#expandContent').click();
    flush();
    assertTrue(isChildVisible(
        /** @type {!HTMLElement} */ (page.$$('#advancedContentList')),
        '#protected-content'));
  });
});
