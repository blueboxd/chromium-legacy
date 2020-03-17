// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('CrSettingsCookiesPageTest', function() {
  /** @type {TestSiteSettingsPrefsBrowserProxy} */
  let siteSettingsBrowserProxy;

  /** @type {settings.TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {SettingsSecurityPageElement} */
  let page;

  /** @type {Element} */
  let clearOnExit;

  /** @type {Element} */
  let networkPrediction;

  /** @type {Element} */
  let allowAll;

  /** @type {Element} */
  let blockThirdPartyIncognito;

  /** @type {Element} */
  let blockThirdParty;

  /** @type {Element} */
  let blockAll;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    settings.MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ =
        siteSettingsBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-cookies-page');
    page.prefs = {
      profile: {
        cookie_controls_mode: {value: 0},
        block_third_party_cookies: {value: false},
      },
    };
    document.body.appendChild(page);
    Polymer.dom.flush();

    allowAll = page.$$('#allowAll');
    blockThirdPartyIncognito = page.$$('#blockThirdPartyIncognito');
    blockThirdParty = page.$$('#blockThirdParty');
    blockAll = page.$$('#blockAll');
    clearOnExit = page.$$('#clearOnExit');
    networkPrediction = page.$$('#networkPrediction');
  });

  teardown(function() {
    page.remove();
  });

  /**
   * Updates the test proxy with the desired content setting for cookies.
   * @param {settings.ContentSetting} setting
   */
  async function updateTestCookieContentSetting(setting) {
    const defaultPrefs = test_util.createSiteSettingsPrefs(
        [test_util.createContentSettingTypeToValuePair(
            settings.ContentSettingsTypes.COOKIES,
            test_util.createDefaultContentSetting({
              setting: setting,
            }))],
        []);
    siteSettingsBrowserProxy.setPrefs(defaultPrefs);
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    siteSettingsBrowserProxy.reset();
    Polymer.dom.flush();
  }

  test('ChangingCookieSettings', async function() {
    // Each radio button updates two preferences and sets a content setting
    // based on the state of the clear on exit toggle. This enumerates the
    // expected behavior for each radio button for testing.
    const testList = [
      {
        element: blockAll,
        updates: {
          contentSetting: settings.ContentSetting.BLOCK,
          cookieControlsMode: settings.CookieControlsMode.ENABLED,
          blockThirdParty: true,
          clearOnExitForcedOff: true,
        },
      },
      {
        element: blockThirdParty,
        updates: {
          contentSetting: settings.ContentSetting.ALLOW,
          cookieControlsMode: settings.CookieControlsMode.ENABLED,
          blockThirdParty: true,
          clearOnExitForcedOff: false,
        },
      },
      {
        element: blockThirdPartyIncognito,
        updates: {
          contentSetting: settings.ContentSetting.ALLOW,
          cookieControlsMode: settings.CookieControlsMode.INCOGNITO_ONLY,
          blockThirdParty: false,
          clearOnExitForcedOff: false,
        },
      },
      {
        element: allowAll,
        updates: {
          contentSetting: settings.ContentSetting.ALLOW,
          cookieControlsMode: settings.CookieControlsMode.DISABLED,
          blockThirdParty: false,
          clearOnExitForcedOff: false,
        },
      }
    ];
    await updateTestCookieContentSetting(settings.ContentSetting.ALLOW);

    for (const test of testList) {
      test.element.click();
      let update = await siteSettingsBrowserProxy.whenCalled(
          'setDefaultValueForContentType');
      Polymer.dom.flush();
      assertEquals(update[0], settings.ContentSettingsTypes.COOKIES);
      assertEquals(update[1], test.updates.contentSetting);
      assertEquals(
          page.prefs.profile.cookie_controls_mode.value,
          test.updates.cookieControlsMode);
      assertEquals(
          page.prefs.profile.block_third_party_cookies.value,
          test.updates.blockThirdParty);

      // Calls to setDefaultValueForContentType don't actually update the test
      // proxy internals, so we need to manually update them.
      await updateTestCookieContentSetting(test.updates.contentSetting);
      assertEquals(clearOnExit.disabled, test.updates.clearOnExitForcedOff);
      siteSettingsBrowserProxy.reset();

      if (!test.updates.clearOnExitForcedOff) {
        clearOnExit.click();
        update = await siteSettingsBrowserProxy.whenCalled(
            'setDefaultValueForContentType');
        assertEquals(update[0], settings.ContentSettingsTypes.COOKIES);
        assertEquals(update[1], settings.ContentSetting.SESSION_ONLY);
        siteSettingsBrowserProxy.reset();
        clearOnExit.checked = false;
      }
    }
  });

  test('RespectChangedCookieSetting_ContentSetting', async function() {
    await updateTestCookieContentSetting(settings.ContentSetting.BLOCK);
    assertTrue(blockAll.checked);
    assertFalse(clearOnExit.checked);
    assertTrue(clearOnExit.disabled);
    siteSettingsBrowserProxy.reset();

    await updateTestCookieContentSetting(settings.ContentSetting.ALLOW);
    assertTrue(allowAll.checked);
    assertFalse(clearOnExit.checked);
    assertFalse(clearOnExit.disabled);
    siteSettingsBrowserProxy.reset();

    await updateTestCookieContentSetting(settings.ContentSetting.SESSION_ONLY);
    assertTrue(allowAll.checked);
    assertTrue(clearOnExit.checked);
    assertFalse(clearOnExit.disabled);
    siteSettingsBrowserProxy.reset();
  });

  test('RespectChangedCookieSetting_CookieControlPref', async function() {
    page.set(
        'prefs.profile.cookie_controls_mode.value',
        settings.CookieControlsMode.INCOGNITO_ONLY);
    Polymer.dom.flush();
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    assertTrue(blockThirdPartyIncognito.checked);
    assertFalse(clearOnExit.checked);
    assertFalse(clearOnExit.disabled);
  });

  test('RespectChangedCookieSetting_BlockThirdPartyPref', async function() {
    page.set('prefs.profile.block_third_party_cookies.value', true);
    Polymer.dom.flush();
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    assertTrue(blockThirdParty.checked);
    assertFalse(clearOnExit.checked);
    assertFalse(clearOnExit.disabled);
  });

  test('ElementVisibility', async function() {
    await test_util.flushTasks();
    assertTrue(test_util.isChildVisible(page, '#clearOnExit'));
    assertTrue(test_util.isChildVisible(page, '#doNotTrack'));
    assertTrue(test_util.isChildVisible(page, '#networkPrediction'));
  });

  test('NetworkPredictionClickRecorded', async function() {
    networkPrediction.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(
        settings.PrivacyElementInteractions.NETWORK_PREDICTION, result);
  });
});
