// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    testElement = document.createElement('settings-recent-site-permissions');
    document.body.appendChild(testElement);
    Polymer.dom.flush();
  });

  test('No recent permissions', async function() {
    browserProxy.setResultFor('getRecentSitePermissions', Promise.resolve([]));
    await testElement.populateList();
    assertTrue(test_util.isVisible(testElement, '#noPermissionsText'));
  });

  test('Various recent permissions', async function() {
    const mock_data = Promise.resolve([
      {
        origin: 'https://bar.com',
        incognito: true,
        recentPermissions:
            [{setting: settings.ContentSetting.BLOCK, displayName: 'location'}]
      },
      {
        origin: 'https://bar.com',
        recentPermissions: [
          {setting: settings.ContentSetting.ALLOW, displayName: 'notifications'}
        ]
      },
      {
        origin: 'http://foo.com',
        recentPermissions: [
          {setting: settings.ContentSetting.BLOCK, displayName: 'popups'}, {
            setting: settings.ContentSetting.BLOCK,
            displayName: 'clipboard',
            source: settings.SiteSettingSource.EMBARGO
          }
        ]
      },
    ]);
    browserProxy.setResultFor('getRecentSitePermissions', mock_data);

    await testElement.populateList();
    assertFalse(testElement.noRecentPermissions);
    assertFalse(test_util.isChildVisible(testElement, '#noPermissionsText'));

    const siteEntries = testElement.shadowRoot.querySelectorAll('.link-button');
    assertEquals(3, siteEntries.length);

    const incognitoIcons =
        testElement.shadowRoot.querySelectorAll('.incognito-icon');
    assertTrue(test_util.isVisible(incognitoIcons[0]));
    assertFalse(test_util.isVisible(incognitoIcons[1]));
    assertFalse(test_util.isVisible(incognitoIcons[2]));
  });
});
