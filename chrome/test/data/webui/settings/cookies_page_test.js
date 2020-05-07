// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, ContentSettingsTypes, CookieControlsMode, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, Router, routes} from 'chrome://settings/settings.js';
import {TestMetricsBrowserProxy} from 'chrome://test/settings/test_metrics_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting,createRawSiteException,createSiteSettingsPrefs} from 'chrome://test/settings/test_util.js';
import {flushTasks, isChildVisible, isVisible} from 'chrome://test/test_util.m.js';

// clang-format on

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

  /** @type {array<!Element>} */
  let radioButtons;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      improvedCookieControlsEnabled: true,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = siteSettingsBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-cookies-page');
    page.prefs = {
      profile: {
        cookie_controls_mode: {value: 0},
        block_third_party_cookies: {value: false},
      },
    };
    document.body.appendChild(page);
    flush();

    allowAll = page.$$('#allowAll');
    blockThirdPartyIncognito = page.$$('#blockThirdPartyIncognito');
    blockThirdParty = page.$$('#blockThirdParty');
    blockAll = page.$$('#blockAll');
    clearOnExit = page.$$('#clearOnExit');
    networkPrediction = page.$$('#networkPrediction');
    radioButtons =
        [allowAll, blockThirdPartyIncognito, blockThirdParty, blockAll];
  });

  teardown(function() {
    page.remove();
  });

  /**
   * Updates the test proxy with the desired content setting for cookies.
   * @param {ContentSetting} setting
   */
  async function updateTestCookieContentSetting(setting) {
    const defaultPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.COOKIES, createDefaultContentSetting({
              setting: setting,
            }))],
        []);
    siteSettingsBrowserProxy.setPrefs(defaultPrefs);
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    siteSettingsBrowserProxy.reset();
    flush();
  }

  test('ChangingCookieSettings', async function() {
    // Each radio button updates two preferences and sets a content setting
    // based on the state of the clear on exit toggle. This enumerates the
    // expected behavior for each radio button for testing.
    const testList = [
      {
        element: blockAll,
        updates: {
          contentSetting: ContentSetting.BLOCK,
          cookieControlsMode: CookieControlsMode.ENABLED,
          blockThirdParty: true,
          clearOnExitForcedOff: true,
        },
      },
      {
        element: blockThirdParty,
        updates: {
          contentSetting: ContentSetting.ALLOW,
          cookieControlsMode: CookieControlsMode.ENABLED,
          blockThirdParty: true,
          clearOnExitForcedOff: false,
        },
      },
      {
        element: blockThirdPartyIncognito,
        updates: {
          contentSetting: ContentSetting.ALLOW,
          cookieControlsMode: CookieControlsMode.INCOGNITO_ONLY,
          blockThirdParty: false,
          clearOnExitForcedOff: false,
        },
      },
      {
        element: allowAll,
        updates: {
          contentSetting: ContentSetting.ALLOW,
          cookieControlsMode: CookieControlsMode.DISABLED,
          blockThirdParty: false,
          clearOnExitForcedOff: false,
        },
      }
    ];
    await updateTestCookieContentSetting(ContentSetting.ALLOW);

    for (const test of testList) {
      test.element.click();
      let update = await siteSettingsBrowserProxy.whenCalled(
          'setDefaultValueForContentType');
      flush();
      assertEquals(update[0], ContentSettingsTypes.COOKIES);
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
        assertEquals(update[0], ContentSettingsTypes.COOKIES);
        assertEquals(update[1], ContentSetting.SESSION_ONLY);
        siteSettingsBrowserProxy.reset();
        clearOnExit.checked = false;
      }
    }
  });

  test('RespectChangedCookieSetting_ContentSetting', async function() {
    await updateTestCookieContentSetting(ContentSetting.BLOCK);
    assertTrue(blockAll.checked);
    assertFalse(clearOnExit.checked);
    assertTrue(clearOnExit.disabled);
    siteSettingsBrowserProxy.reset();

    await updateTestCookieContentSetting(ContentSetting.ALLOW);
    assertTrue(allowAll.checked);
    assertFalse(clearOnExit.checked);
    assertFalse(clearOnExit.disabled);
    siteSettingsBrowserProxy.reset();

    await updateTestCookieContentSetting(ContentSetting.SESSION_ONLY);
    assertTrue(allowAll.checked);
    assertTrue(clearOnExit.checked);
    assertFalse(clearOnExit.disabled);
    siteSettingsBrowserProxy.reset();
  });

  test('RespectChangedCookieSetting_CookieControlPref', async function() {
    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.INCOGNITO_ONLY);
    flush();
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    assertTrue(blockThirdPartyIncognito.checked);
    assertFalse(clearOnExit.checked);
    assertFalse(clearOnExit.disabled);
  });

  test('RespectChangedCookieSetting_BlockThirdPartyPref', async function() {
    page.set('prefs.profile.block_third_party_cookies.value', true);
    flush();
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    assertTrue(blockThirdParty.checked);
    assertFalse(clearOnExit.checked);
    assertFalse(clearOnExit.disabled);
  });

  test('ElementVisibility', async function() {
    await flushTasks();
    assertTrue(isChildVisible(page, '#clearOnExit'));
    assertTrue(isChildVisible(page, '#doNotTrack'));
    assertTrue(isChildVisible(page, '#networkPrediction'));
    // Ensure that with the improvedCookieControls flag enabled that the block
    // third party cookies radio is visible.
    assertTrue(isVisible(blockThirdPartyIncognito));
  });

  test('NetworkPredictionClickRecorded', async function() {
    networkPrediction.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.NETWORK_PREDICTION, result);
  });

  test('CookiesSiteDataSubpageRoute', function() {
    page.$$('#site-data-trigger').click();
    assertEquals(
        Router.getInstance().getCurrentRoute(), routes.SITE_SETTINGS_SITE_DATA);
  });

  test('CookiesRadioClicksRecorded', async function() {
    allowAll.click();
    let result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_ALL, result);

    testMetricsBrowserProxy.reset();

    blockThirdPartyIncognito.click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_INCOGNITO, result);

    testMetricsBrowserProxy.reset();

    blockThirdParty.click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_THIRD, result);

    testMetricsBrowserProxy.reset();

    blockAll.click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_BLOCK, result);
  });

  test('CookiseSessionOnlyClickRecorded', async function() {
    clearOnExit.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_SESSION, result);
  });


  test('CookieSettingExceptions_Search', async function() {
    const exceptionPrefs = createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.COOKIES,
          [
            createRawSiteException('http://foo-block.com', {
              embeddingOrigin: '',
              setting: ContentSetting.BLOCK,
            }),
            createRawSiteException('http://foo-allow.com', {
              embeddingOrigin: '',
            }),
            createRawSiteException('http://foo-session.com', {
              embeddingOrigin: '',
              setting: ContentSetting.SESSION_ONLY,
            }),
          ]),
    ]);
    page.searchTerm = 'foo';
    siteSettingsBrowserProxy.setPrefs(exceptionPrefs);
    await siteSettingsBrowserProxy.whenCalled('getExceptionList');
    flush();

    const exceptionLists = page.shadowRoot.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);

    for (const list of exceptionLists) {
      assertTrue(isChildVisible(list, 'site-list-entry'));
    }

    page.searchTerm = 'unrelated.com';
    flush();

    for (const list of exceptionLists) {
      assertFalse(isChildVisible(list, 'site-list-entry'));
    }
  });

  test('CookieControls_ManagedState', async function() {
    const managedControlState = {
      allowAll: {disabled: true, indicator: 'devicePolicy'},
      blockThirdPartyIncognito: {disabled: true, indicator: 'devicePolicy'},
      blockThirdParty: {disabled: true, indicator: 'devicePolicy'},
      blockAll: {disabled: true, indicator: 'devicePolicy'},
      sessionOnly: {disabled: true, indicator: 'devicePolicy'},
    };
    const managedPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.COOKIES, createDefaultContentSetting({
              setting: ContentSetting.SESSION_ONLY,
              source: SiteSettingSource.POLICY
            }))],
        []);
    siteSettingsBrowserProxy.setCookieControlsManagedState(managedControlState);
    siteSettingsBrowserProxy.setPrefs(managedPrefs);
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    await siteSettingsBrowserProxy.whenCalled('getCookieControlsManagedState');
    flush();

    // Check the four radio buttons are correctly indicating they are managed.
    for (const button of radioButtons) {
      assertTrue(button.disabled);
      assertEquals(button.policyIndicatorType, 'devicePolicy');
    }

    // Check the clear on exit toggle is correctly indicating it is managed.
    assertTrue(clearOnExit.checked);
    assertTrue(clearOnExit.controlDisabled());
    assertTrue(isChildVisible(clearOnExit, 'cr-policy-pref-indicator'));
    let exceptionLists = page.shadowRoot.querySelectorAll('site-list');

    // Check all exception lists are read only.
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertTrue(!!list.readOnlyList);
    }

    // Revert to an unmanaged state and ensure all controls return to unmanged.
    const unmanagedControlState = {
      allowAll: {disabled: false, indicator: 'none'},
      blockThirdPartyIncognito: {disabled: false, indicator: 'none'},
      blockThirdParty: {disabled: false, indicator: 'none'},
      blockAll: {disabled: false, indicator: 'none'},
      sessionOnly: {disabled: false, indicator: 'none'},
    };
    const unmanagedPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.COOKIES, createDefaultContentSetting({
              setting: ContentSetting.ALLOW,
            }))],
        []);
    siteSettingsBrowserProxy.reset();
    siteSettingsBrowserProxy.setCookieControlsManagedState(
        unmanagedControlState);
    siteSettingsBrowserProxy.setPrefs(unmanagedPrefs);
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    await siteSettingsBrowserProxy.whenCalled('getCookieControlsManagedState');

    // Check the four radio buttons no longer indicate they are managed.
    for (const button of radioButtons) {
      assertFalse(button.disabled);
      assertEquals(button.policyIndicatorType, 'none');
    }

    // Check the clear on exit toggle no longer indicates it is managed.
    assertFalse(clearOnExit.checked);
    assertFalse(clearOnExit.controlDisabled());
    assertFalse(isChildVisible(clearOnExit, 'cr-policy-pref-indicator'));

    // Check all exception lists are no longer read only.
    exceptionLists = page.shadowRoot.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertFalse(!!list.readOnlyList);
    }
  });
});

suite('CrSettingsCookiesPageTest_ImprovedCookieControlsDisabled', function() {
  /** @type {TestSiteSettingsPrefsBrowserProxy} */
  let siteSettingsBrowserProxy;

  /** @type {SettingsSecurityPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      improvedCookieControlsEnabled: false,
    });
  });

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = siteSettingsBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-cookies-page');
    page.prefs = {
      profile: {
        cookie_controls_mode: {value: 0},
        block_third_party_cookies: {value: false},
      },
    };
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('BlockThirdPartyRadio_Hidden', function() {
    assertFalse(isChildVisible(page, '#blockThirdPartyIncognito'));
  });

  test('BlockThirdPartyRadio_NotSelected', async function() {
    // Create a preference state that would select the removed radio button
    // and ensure the correct radio button is instead selected.
    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.INCOGNITO_ONLY);
    flush();
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');

    assertTrue(page.$$('#allowAll').checked);
  });
});
