// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SiteSettingsPage', function() {
  /** @type {settings.TestMetricsBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsSiteSettingsPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: false,
    });
  });

  function setupPage() {
    testBrowserProxy = new TestMetricsBrowserProxy();
    settings.MetricsBrowserProxyImpl.instance_ = testBrowserProxy;
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

  async function testClicks(listElement) {
    const triggers = listElement.shadowRoot.querySelectorAll('cr-link-row');
    assertTrue(triggers.length > 0);
    const domRepeat = listElement.$$('dom-repeat').template;
    for (const trigger of triggers) {
      const data = Polymer.Templatize.modelForElement(domRepeat, trigger);
      assertTrue(!!data);
      trigger.click();
      const result =
          await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
      assertEquals(
          settings.SettingsPageInteractions[`PRIVACY_${data.item.route}`],
          result);
      settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
      testBrowserProxy.reset();
    }
  }

  test('LogAllSiteSettingsPageClicks', async function() {
    // Test the allSites case.
    page.$$('#allSites').click();
    const result =
        await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(
        settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_ALL, result);
    settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
    testBrowserProxy.reset();

    // Test all remaining items.
    const lists =
        page.shadowRoot.querySelectorAll('settings-site-settings-list');
    assertEquals(1, lists.length);
    await testClicks(lists[0]);
  });

  test('LogAllSiteSettingsPageClicks_Redesign', async function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: true,
    });
    setupPage();

    // Test the allSites case.
    page.$$('#allSites').click();
    const result =
        await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(
        settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_ALL, result);
    settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
    testBrowserProxy.reset();

    // Test all remaining items (which are spread over four separate lists).
    const lists =
        page.shadowRoot.querySelectorAll('settings-site-settings-list');
    assertEquals(4, lists.length);
    for (const list of lists) {
      await testClicks(list);
    }
  });
});
