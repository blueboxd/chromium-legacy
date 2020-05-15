// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, ContentSettingsTypes, CookieControlsMode, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, Router, routes} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {flushTasks, isChildVisible, isVisible} from '../test_util.m.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting,createRawSiteException,createSiteSettingsPrefs} from './test_util.js';

// clang-format on

suite('CrSettingsCookiesPageTest', function() {
  /** @type {!TestSiteSettingsPrefsBrowserProxy} */
  let siteSettingsBrowserProxy;

  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {!SettingsCookiesPageElement} */
  let page;

  /** @type {!SettingsToggleButtonElement} */
  let clearOnExit;

  /** @type {!SettingsToggleButtonElement} */
  let networkPrediction;

  /** @type {!SettingsCollapseRadioButtonElement} */
  let allowAll;

  /** @type {!SettingsCollapseRadioButtonElement} */
  let blockThirdPartyIncognito;

  /** @type {!SettingsCollapseRadioButtonElement} */
  let blockThirdParty;

  /** @type {!SettingsCollapseRadioButtonElement} */
  let blockAll;

  /** @type {!Array<!SettingsCollapseRadioButtonElement>} */
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
    document.body.innerHTML = '';
    page = /** @type {!SettingsCookiesPageElement} */ (
        document.createElement('settings-cookies-page'));
    page.prefs = {
      profile: {
        cookie_controls_mode: {value: 0},
        block_third_party_cookies: {value: false},
      },
    };
    document.body.appendChild(page);
    flush();

    radioButtons = /** @type {!Array<!SettingsCollapseRadioButtonElement>} */ ([
      page.$$('#allowAll'),
      page.$$('#blockThirdPartyIncognito'),
      page.$$('#blockThirdParty'),
      page.$$('#blockAll'),
    ]);
    [allowAll, blockThirdPartyIncognito, blockThirdParty, blockAll] =
        radioButtons;

    clearOnExit =
        /** @type {!SettingsToggleButtonElement} */ (page.$$('#clearOnExit'));
    networkPrediction = /** @type {!SettingsToggleButtonElement} */ (
        page.$$('#networkPrediction'));
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
          cookieControlsMode: CookieControlsMode.BLOCK_THIRD_PARTY,
          blockThirdParty: true,
        },
      },
      {
        element: blockThirdParty,
        updates: {
          contentSetting: ContentSetting.ALLOW,
          cookieControlsMode: CookieControlsMode.BLOCK_THIRD_PARTY,
          blockThirdParty: true,
        },
      },
      {
        element: blockThirdPartyIncognito,
        updates: {
          contentSetting: ContentSetting.ALLOW,
          cookieControlsMode: CookieControlsMode.INCOGNITO_ONLY,
          blockThirdParty: false,
        },
      },
      {
        element: allowAll,
        updates: {
          contentSetting: ContentSetting.ALLOW,
          cookieControlsMode: CookieControlsMode.OFF,
          blockThirdParty: false,
        },
      }
    ];
    await updateTestCookieContentSetting(ContentSetting.ALLOW);

    for (const test of testList) {
      test.element.click();
      const update = await siteSettingsBrowserProxy.whenCalled(
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
      siteSettingsBrowserProxy.reset();
    }
  });

  test('RespectChangedCookieSetting_ContentSetting', async function() {
    await updateTestCookieContentSetting(ContentSetting.BLOCK);
    assertTrue(blockAll.checked);
    siteSettingsBrowserProxy.reset();

    await updateTestCookieContentSetting(ContentSetting.ALLOW);
    assertTrue(allowAll.checked);
    siteSettingsBrowserProxy.reset();

    await updateTestCookieContentSetting(ContentSetting.SESSION_ONLY);
    assertTrue(allowAll.checked);
    siteSettingsBrowserProxy.reset();
  });

  test('RespectChangedCookieSetting_CookieControlPref', async function() {
    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.INCOGNITO_ONLY);
    flush();
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    assertTrue(blockThirdPartyIncognito.checked);
  });

  test('RespectChangedCookieSetting_BlockThirdPartyPref', async function() {
    page.set('prefs.profile.block_third_party_cookies.value', true);
    flush();
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    assertTrue(blockThirdParty.checked);
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

    const exceptionLists = /** @type {!NodeList<!SiteListElement>} */ (
        page.shadowRoot.querySelectorAll('site-list'));
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
      allowAll:
          {disabled: true, indicator: CrPolicyIndicatorType.DEVICE_POLICY},
      blockThirdPartyIncognito:
          {disabled: true, indicator: CrPolicyIndicatorType.DEVICE_POLICY},
      blockThirdParty:
          {disabled: true, indicator: CrPolicyIndicatorType.DEVICE_POLICY},
      blockAll:
          {disabled: true, indicator: CrPolicyIndicatorType.DEVICE_POLICY},
      sessionOnly:
          {disabled: true, indicator: CrPolicyIndicatorType.DEVICE_POLICY},
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

    // Check all exception lists are read only.
    let exceptionLists = page.shadowRoot.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertTrue(!!list.readOnlyList);
    }

    // Revert to an unmanaged state and ensure all controls return to unmanged.
    const unmanagedControlState = {
      allowAll: {disabled: false, indicator: CrPolicyIndicatorType.NONE},
      blockThirdPartyIncognito:
          {disabled: false, indicator: CrPolicyIndicatorType.NONE},
      blockThirdParty: {disabled: false, indicator: CrPolicyIndicatorType.NONE},
      blockAll: {disabled: false, indicator: CrPolicyIndicatorType.NONE},
      sessionOnly: {disabled: false, indicator: CrPolicyIndicatorType.NONE},
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

  /** @type {!SettingsCookiesPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      improvedCookieControlsEnabled: false,
    });
  });

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = siteSettingsBrowserProxy;
    document.body.innerHTML = '';
    page = /** @type {!SettingsCookiesPageElement} */ (
        document.createElement('settings-cookies-page'));
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
