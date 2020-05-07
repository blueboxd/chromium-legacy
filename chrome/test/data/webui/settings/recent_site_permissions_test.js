// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting,SiteSettingSource,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {routes} from 'chrome://settings/settings.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {isChildVisible,isVisible} from 'chrome://test/test_util.m.js';

// clang-format on

suite('CrSettingsRecentSitePermissionsTest', function() {
  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  /** @type {SettingsRecentSitePermissionsElement} */
  let testElement;

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    testElement = document.createElement('settings-recent-site-permissions');
    document.body.appendChild(testElement);
    flush();
  });

  teardown(function() {
    testElement.remove();
  });

  test('No recent permissions', async function() {
    browserProxy.setRecentSitePermissions([]);
    testElement.currentRouteChanged(routes.SITE_SETTINGS);
    await browserProxy.whenCalled('getRecentSitePermissions');
    flush();
    assertTrue(isVisible(testElement, '#noPermissionsText'));
  });

  test('Various recent permissions', async function() {
    const mockData = Promise.resolve([
      {
        origin: 'https://bar.com',
        incognito: true,
        recentPermissions:
            [{setting: ContentSetting.BLOCK, displayName: 'location'}]
      },
      {
        origin: 'https://bar.com',
        recentPermissions:
            [{setting: ContentSetting.ALLOW, displayName: 'notifications'}]
      },
      {
        origin: 'http://foo.com',
        recentPermissions: [
          {setting: ContentSetting.BLOCK, displayName: 'popups'}, {
            setting: ContentSetting.BLOCK,
            displayName: 'clipboard',
            source: SiteSettingSource.EMBARGO
          }
        ]
      },
    ]);
    browserProxy.setRecentSitePermissions(mockData);
    testElement.currentRouteChanged(routes.SITE_SETTINGS);
    await browserProxy.whenCalled('getRecentSitePermissions');
    flush();

    assertFalse(testElement.noRecentPermissions);
    assertFalse(isChildVisible(testElement, '#noPermissionsText'));

    const siteEntries = testElement.shadowRoot.querySelectorAll('.link-button');
    assertEquals(3, siteEntries.length);

    const incognitoIcons =
        testElement.shadowRoot.querySelectorAll('.incognito-icon');
    assertTrue(isVisible(incognitoIcons[0]));
    assertFalse(isVisible(incognitoIcons[1]));
    assertFalse(isVisible(incognitoIcons[2]));
  });
});
