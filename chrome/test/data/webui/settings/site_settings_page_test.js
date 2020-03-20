// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ =
        siteSettingsBrowserProxy;
    siteSettingsBrowserProxy.setResultFor(
        'getCookieSettingDescription', Promise.resolve(testLabels[0]));
    PolymerTest.clearBody();
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    Polymer.dom.flush();
  }

  setup(setupPage);

  teardown(function() {
    page.remove();
  });

  test('DefaultLabels', function() {
    assertEquals(
        'a',
        settings.defaultSettingLabel(settings.ContentSetting.ALLOW, 'a', 'b'));
    assertEquals(
        'b',
        settings.defaultSettingLabel(settings.ContentSetting.BLOCK, 'a', 'b'));
    assertEquals(
        'a',
        settings.defaultSettingLabel(
            settings.ContentSetting.ALLOW, 'a', 'b', 'c'));
    assertEquals(
        'b',
        settings.defaultSettingLabel(
            settings.ContentSetting.BLOCK, 'a', 'b', 'c'));
    assertEquals(
        'c',
        settings.defaultSettingLabel(
            settings.ContentSetting.SESSION_ONLY, 'a', 'b', 'c'));
    assertEquals(
        'c',
        settings.defaultSettingLabel(
            settings.ContentSetting.DEFAULT, 'a', 'b', 'c'));
    assertEquals(
        'c',
        settings.defaultSettingLabel(
            settings.ContentSetting.ASK, 'a', 'b', 'c'));
    assertEquals(
        'c',
        settings.defaultSettingLabel(
            settings.ContentSetting.IMPORTANT_CONTENT, 'a', 'b', 'c'));
  });

  test('CookiesLinkRowSublabel', async function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: false,
    });
    setupPage();
    const allSettingsList = page.$$('#allSettingsList');
    await test_util.eventToPromise(
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
    Polymer.dom.flush();
    const cookiesLinkRow = page.$$('#basicContentList').$$('#cookies');
    assertEquals(testLabels[0], cookiesLinkRow.subLabel);

    cr.webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(testLabels[1], cookiesLinkRow.subLabel);
  });
});
