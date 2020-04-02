// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {ContentSetting, SiteSettingsPrefsBrowserProxyImpl, CookieControlsMode, ContentSettingsTypes, SiteSettingSource} from 'chrome://settings/lazy_load.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';
// #import {TestMetricsBrowserProxy} from 'chrome://test/settings/test_metrics_browser_proxy.m.js';
// #import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.m.js';
// #import {createRawSiteException, createDefaultContentSetting,createSiteSettingsPrefs,createContentSettingTypeToValuePair} from 'chrome://test/settings/test_util.m.js';
// #import {isChildVisible, isVisible, flushTasks} from 'chrome://test/test_util.m.js';
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
    radioButtons =
        [allowAll, blockThirdPartyIncognito, blockThirdParty, blockAll];
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
    // Ensure that with the improvedCookieControls flag enabled that the block
    // third party cookies radio is visible.
    assertTrue(test_util.isVisible(blockThirdPartyIncognito));
  });

  test('NetworkPredictionClickRecorded', async function() {
    networkPrediction.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(
        settings.PrivacyElementInteractions.NETWORK_PREDICTION, result);
  });

  test('CookieSettingExceptions_Search', async function() {
    const exceptionPrefs = test_util.createSiteSettingsPrefs([], [
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.COOKIES,
          [
            test_util.createRawSiteException('http://foo-block.com', {
              embeddingOrigin: '',
              setting: settings.ContentSetting.BLOCK,
            }),
            test_util.createRawSiteException('http://foo-allow.com', {
              embeddingOrigin: '',
            }),
            test_util.createRawSiteException('http://foo-session.com', {
              embeddingOrigin: '',
              setting: settings.ContentSetting.SESSION_ONLY,
            }),
          ]),
    ]);
    page.searchTerm = 'foo';
    siteSettingsBrowserProxy.setPrefs(exceptionPrefs);
    await siteSettingsBrowserProxy.whenCalled('getExceptionList');
    Polymer.dom.flush();

    const exceptionLists = page.shadowRoot.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);

    for (const list of exceptionLists) {
      assertTrue(test_util.isChildVisible(list, 'site-list-entry'));
    }

    page.searchTerm = 'unrelated.com';
    Polymer.dom.flush();

    for (const list of exceptionLists) {
      assertFalse(test_util.isChildVisible(list, 'site-list-entry'));
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
    const managedPrefs = test_util.createSiteSettingsPrefs(
        [test_util.createContentSettingTypeToValuePair(
            settings.ContentSettingsTypes.COOKIES,
            test_util.createDefaultContentSetting({
              setting: settings.ContentSetting.SESSION_ONLY,
              source: settings.SiteSettingSource.POLICY
            }))],
        []);
    siteSettingsBrowserProxy.setResultFor(
        'getCookieControlsManagedState', Promise.resolve(managedControlState));
    siteSettingsBrowserProxy.setPrefs(managedPrefs);
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');
    await siteSettingsBrowserProxy.whenCalled('getCookieControlsManagedState');
    Polymer.dom.flush();

    // Check the four radio buttons are correctly indicating they are managed.
    for (const button of radioButtons) {
      assertTrue(button.disabled);
      assertEquals(button.policyIndicatorType, 'devicePolicy');
    }

    // Check the clear on exit toggle is correctly indicating it is managed.
    assertTrue(clearOnExit.checked);
    assertTrue(clearOnExit.controlDisabled());
    assertTrue(
        test_util.isChildVisible(clearOnExit, 'cr-policy-pref-indicator'));
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
    const unmanagedPrefs = test_util.createSiteSettingsPrefs(
        [test_util.createContentSettingTypeToValuePair(
            settings.ContentSettingsTypes.COOKIES,
            test_util.createDefaultContentSetting({
              setting: settings.ContentSetting.ALLOW,
            }))],
        []);
    siteSettingsBrowserProxy.reset();
    siteSettingsBrowserProxy.setResultFor(
        'getCookieControlsManagedState',
        Promise.resolve(unmanagedControlState));
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
    assertFalse(
        test_util.isChildVisible(clearOnExit, 'cr-policy-pref-indicator'));

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
  });

  teardown(function() {
    page.remove();
  });

  test('BlockThirdPartyRadio_Hidden', function() {
    assertFalse(test_util.isChildVisible(page, '#blockThirdPartyIncognito'));
  });

  test('BlockThirdPartyRadio_NotSelected', async function() {
    // Create a preference state that would select the removed radio button
    // and ensure the correct radio button is instead selected.
    page.set(
        'prefs.profile.cookie_controls_mode.value',
        settings.CookieControlsMode.INCOGNITO_ONLY);
    Polymer.dom.flush();
    await siteSettingsBrowserProxy.whenCalled('getDefaultValueForContentType');

    assertTrue(page.$$('#allowAll').checked);
  });
});
