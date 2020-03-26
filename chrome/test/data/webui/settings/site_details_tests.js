// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {ChooserType,ContentSetting,ContentSettingsTypes,SiteSettingSource,SiteSettingsPrefsBrowserProxyImpl,WebsiteUsageBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
// #import {createContentSettingTypeToValuePair,createRawChooserException,createRawSiteException,createSiteSettingsPrefs} from 'chrome://test/settings/test_util.m.js';
// #import {flush,Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// #import {listenOnce} from 'chrome://resources/js/util.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {Route,Router,routes} from 'chrome://settings/settings.js';
// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// #import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.m.js';
// clang-format on

class TestWebsiteUsageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['clearUsage', 'fetchUsageTotal']);
  }

  /** @override */
  fetchUsageTotal(host) {
    this.methodCalled('fetchUsageTotal', host);
  }

  /** @override */
  clearUsage(origin) {
    this.methodCalled('clearUsage', origin);
  }
}

/** @fileoverview Suite of tests for site-details. */
suite('SiteDetails', function() {
  /**
   * A site list element created before each test.
   * @type {SiteDetails}
   */
  let testElement;

  /**
   * An example pref with 1 pref in each category.
   * @type {SiteSettingsPref}
   */
  let prefs;

  /**
   * The mock site settings prefs proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  /**
   * The mock website usage proxy object to use during test.
   * @type {TestWebsiteUsageBrowserProxy}
   */
  let websiteUsageProxy;

  // Initialize a site-details before each test.
  setup(function() {
    prefs = test_util.createSiteSettingsPrefs([], [
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.COOKIES,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.IMAGES,
          [test_util.createRawSiteException('https://foo.com:443', {
            source: settings.SiteSettingSource.DEFAULT,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.JAVASCRIPT,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.SOUND,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.PLUGINS,
          [test_util.createRawSiteException('https://foo.com:443', {
            source: settings.SiteSettingSource.EXTENSION,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.POPUPS,
          [test_util.createRawSiteException('https://foo.com:443', {
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.DEFAULT,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.GEOLOCATION,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.NOTIFICATIONS,
          [test_util.createRawSiteException('https://foo.com:443', {
            setting: settings.ContentSetting.ASK,
            source: settings.SiteSettingSource.POLICY,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.MIC,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.CAMERA,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.BACKGROUND_SYNC,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.MIDI_DEVICES,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.PROTECTED_CONTENT,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.ADS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.CLIPBOARD,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.SENSORS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.PAYMENT_HANDLER,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.SERIAL_PORTS,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.BLUETOOTH_SCANNING,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE,
          [test_util.createRawSiteException('https://foo.com:443', {
            setting: settings.ContentSetting.BLOCK,
          })]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.MIXEDSCRIPT,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.HID_DEVICES,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.BLUETOOTH_DEVICES,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.AR,
          [test_util.createRawSiteException('https://foo.com:443')]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.VR,
          [test_util.createRawSiteException('https://foo.com:443')]),

    ], [
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.USB_DEVICES,
          [test_util.createRawChooserException(
              settings.ChooserType.USB_DEVICES,
              [test_util.createRawSiteException('https://foo.com:443')])]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.BLUETOOTH_DEVICES,
          [test_util.createRawChooserException(
              settings.ChooserType.BLUETOOTH_DEVICES,
              [test_util.createRawSiteException('https://foo.com:443')])]),
    ]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    websiteUsageProxy = new TestWebsiteUsageBrowserProxy();
    settings.WebsiteUsageBrowserProxyImpl.instance_ = websiteUsageProxy;

    PolymerTest.clearBody();
  });

  function createSiteDetails(origin) {
    const siteDetailsElement = document.createElement('site-details');
    document.body.appendChild(siteDetailsElement);
    siteDetailsElement.origin = origin;
    settings.Router.getInstance().navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
    return siteDetailsElement;
  }

  test('all site settings are shown', function() {
    // Add ContentsSettingsTypes which are not supposed to be shown on the Site
    // Details page here.
    const nonSiteDetailsContentSettingsTypes = [
      settings.ContentSettingsTypes.COOKIES,
      settings.ContentSettingsTypes.PROTOCOL_HANDLERS,
      settings.ContentSettingsTypes.ZOOM_LEVELS,
    ];
    if (!cr.isChromeOS) {
      nonSiteDetailsContentSettingsTypes.push(
          settings.ContentSettingsTypes.PROTECTED_CONTENT);
    }

    // A list of optionally shown content settings mapped to their loadTimeData
    // flag string.
    const optionalSiteDetailsContentSettingsTypes =
        /** @type {!settings.ContentSettingsType : string} */ ({});
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes.ADS] =
        'enableSafeBrowsingSubresourceFilter';

    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .PAYMENT_HANDLER] =
        'enablePaymentHandlerContentSetting';

    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .NATIVE_FILE_SYSTEM_WRITE] =
        'enableNativeFileSystemWriteContentSetting';
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .MIXEDSCRIPT] =
        'enableInsecureContentContentSetting';
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .BLUETOOTH_SCANNING] =
        'enableExperimentalWebPlatformFeatures';
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .HID_DEVICES] =
        'enableExperimentalWebPlatformFeatures';
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes.AR] =
        'enableWebXrContentSetting';
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes.VR] =
        'enableWebXrContentSetting';
    optionalSiteDetailsContentSettingsTypes[settings.ContentSettingsTypes
                                                .BLUETOOTH_DEVICES] =
        'enableWebBluetoothNewPermissionsBackend';

    const controlledSettingsCount = /** @type{string : int } */ ({});

    controlledSettingsCount['enableSafeBrowsingSubresourceFilter'] = 1;
    controlledSettingsCount['enablePaymentHandlerContentSetting'] = 1;
    controlledSettingsCount['enableNativeFileSystemWriteContentSetting'] = 1;
    controlledSettingsCount['enableInsecureContentContentSetting'] = 1;
    controlledSettingsCount['enableWebXrContentSetting'] = 2;
    controlledSettingsCount['enableExperimentalWebPlatformFeatures'] = 2;
    controlledSettingsCount['enableWebBluetoothNewPermissionsBackend'] = 1;

    browserProxy.setPrefs(prefs);

    // First, explicitly set all the optional settings to false.
    for (const contentSetting in optionalSiteDetailsContentSettingsTypes) {
      const loadTimeDataOverride = {};
      loadTimeDataOverride
          [optionalSiteDetailsContentSettingsTypes[contentSetting]] = false;
      loadTimeData.overrideValues(loadTimeDataOverride);
    }

    // Iterate over each flag in on / off state, assuming that the on state
    // means the content setting will show, and off hides it.
    for (const contentSetting in optionalSiteDetailsContentSettingsTypes) {
      const numContentSettings =
          Object.keys(settings.ContentSettingsTypes).length -
          nonSiteDetailsContentSettingsTypes.length -
          Object.keys(optionalSiteDetailsContentSettingsTypes).length;

      const loadTimeDataOverride = {};
      loadTimeDataOverride
          [optionalSiteDetailsContentSettingsTypes[contentSetting]] = true;
      loadTimeData.overrideValues(loadTimeDataOverride);
      testElement = createSiteDetails('https://foo.com:443');
      assertEquals(
          numContentSettings +
              controlledSettingsCount[optionalSiteDetailsContentSettingsTypes[
                  [contentSetting]]],
          testElement.getCategoryList().length);

      // Check for setting = off at the end to ensure that the setting does
      // not carry over for the next iteration.
      loadTimeDataOverride
          [optionalSiteDetailsContentSettingsTypes[contentSetting]] = false;
      loadTimeData.overrideValues(loadTimeDataOverride);
      testElement = createSiteDetails('https://foo.com:443');
      assertEquals(numContentSettings, testElement.getCategoryList().length);
    }
  });

  test('usage heading shows properly', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    Polymer.dom.flush();
    assertTrue(!!testElement.$$('#usage'));

    // When there's no usage, there should be a string that says so.
    assertEquals('', testElement.storedData_);
    assertFalse(testElement.$$('#noStorage').hidden);
    assertTrue(testElement.$$('#storage').hidden);
    assertTrue(
        testElement.$$('#usage').innerText.indexOf('No usage data') != -1);

    // If there is, check the correct amount of usage is specified.
    testElement.storedData_ = '1 KB';
    assertTrue(testElement.$$('#noStorage').hidden);
    assertFalse(testElement.$$('#storage').hidden);
    assertTrue(testElement.$$('#usage').innerText.indexOf('1 KB') != -1);
  });

  test('storage gets trashed properly', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);

    Polymer.dom.flush();

    // Call onOriginChanged_() manually to simulate a new navigation.
    testElement.currentRouteChanged(settings.Route);
    return Promise
        .all([
          browserProxy.whenCalled('getOriginPermissions'),
          websiteUsageProxy.whenCalled('fetchUsageTotal'),
        ])
        .then(results => {
          const hostRequested = results[1];
          assertEquals('foo.com', hostRequested);
          cr.webUIListenerCallback(
              'usage-total-changed', hostRequested, '1 KB', '10 cookies');
          assertEquals('1 KB', testElement.storedData_);
          assertTrue(testElement.$$('#noStorage').hidden);
          assertFalse(testElement.$$('#storage').hidden);

          testElement.$$('#confirmClearStorage .action-button').click();
          return websiteUsageProxy.whenCalled('clearUsage');
        })
        .then(originCleared => {
          assertEquals('https://foo.com/', originCleared);
        });
  });

  test('cookies gets deleted properly', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);

    // Call onOriginChanged_() manually to simulate a new navigation.
    testElement.currentRouteChanged(settings.Route);
    return Promise
        .all([
          browserProxy.whenCalled('getOriginPermissions'),
          websiteUsageProxy.whenCalled('fetchUsageTotal'),
        ])
        .then(results => {
          // Ensure the mock's methods were called and check usage was cleared
          // on clicking the trash button.
          const hostRequested = results[1];
          assertEquals('foo.com', hostRequested);
          cr.webUIListenerCallback(
              'usage-total-changed', hostRequested, '1 KB', '10 cookies');
          assertEquals('10 cookies', testElement.numCookies_);
          assertTrue(testElement.$$('#noStorage').hidden);
          assertFalse(testElement.$$('#storage').hidden);

          testElement.$$('#confirmClearStorage .action-button').click();
          return websiteUsageProxy.whenCalled('clearUsage');
        })
        .then(originCleared => {
          assertEquals('https://foo.com/', originCleared);
        });
  });

  test('correct pref settings are shown', function() {
    browserProxy.setPrefs(prefs);
    // Make sure all the possible content settings are shown for this test.
    loadTimeData.overrideValues({enableSafeBrowsingSubresourceFilter: true});
    loadTimeData.overrideValues({enablePaymentHandlerContentSetting: true});
    loadTimeData.overrideValues(
        {enableNativeFileSystemWriteContentSetting: true});
    loadTimeData.overrideValues({enableWebXrContentSetting: true});
    testElement = createSiteDetails('https://foo.com:443');

    return browserProxy.whenCalled('isOriginValid')
        .then(() => {
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then(() => {
          testElement.root.querySelectorAll('site-details-permission')
              .forEach((siteDetailsPermission) => {
                if (!cr.isChromeOS &&
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.PROTECTED_CONTENT) {
                  return;
                }

                // Verify settings match the values specified in |prefs|.
                let expectedSetting = settings.ContentSetting.ALLOW;
                let expectedSource = settings.SiteSettingSource.PREFERENCE;
                let expectedMenuValue = settings.ContentSetting.ALLOW;

                // For all the categories with non-user-set 'Allow' preferences,
                // update expected values.
                if (siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.NOTIFICATIONS ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.PLUGINS ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.JAVASCRIPT ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.IMAGES ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes.POPUPS ||
                    siteDetailsPermission.category ==
                        settings.ContentSettingsTypes
                            .NATIVE_FILE_SYSTEM_WRITE) {
                  expectedSetting =
                      prefs.exceptions[siteDetailsPermission.category][0]
                          .setting;
                  expectedSource =
                      prefs.exceptions[siteDetailsPermission.category][0]
                          .source;
                  expectedMenuValue =
                      (expectedSource == settings.SiteSettingSource.DEFAULT) ?
                      settings.ContentSetting.DEFAULT :
                      expectedSetting;
                }
                assertEquals(
                    expectedSetting, siteDetailsPermission.site.setting);
                assertEquals(expectedSource, siteDetailsPermission.site.source);
                assertEquals(
                    expectedMenuValue,
                    siteDetailsPermission.$.permission.value);
              });
        });
  });

  test('show confirmation dialog on reset settings', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    Polymer.dom.flush();

    // Check both cancelling and accepting the dialog closes it.
    ['cancel-button', 'action-button'].forEach(buttonType => {
      testElement.$$('#resetSettingsButton').click();
      assertTrue(testElement.$.confirmResetSettings.open);
      const actionButtonList =
          testElement.$.confirmResetSettings.getElementsByClassName(buttonType);
      assertEquals(1, actionButtonList.length);
      actionButtonList[0].click();
      assertFalse(testElement.$.confirmResetSettings.open);
    });

    // Accepting the dialog will make a call to setOriginPermissions.
    return browserProxy.whenCalled('setOriginPermissions').then((args) => {
      assertEquals(testElement.origin, args[0]);
      assertDeepEquals(testElement.getCategoryList(), args[1]);
      assertEquals(settings.ContentSetting.DEFAULT, args[2]);
    });
  });

  test('show confirmation dialog on clear storage', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');
    Polymer.dom.flush();

    // Check both cancelling and accepting the dialog closes it.
    ['cancel-button', 'action-button'].forEach(buttonType => {
      testElement.$$('#usage cr-button').click();
      assertTrue(testElement.$.confirmClearStorage.open);
      const actionButtonList =
          testElement.$.confirmClearStorage.getElementsByClassName(buttonType);
      assertEquals(1, actionButtonList.length);
      testElement.storedData_ = '';
      actionButtonList[0].click();
      assertFalse(testElement.$.confirmClearStorage.open);
    });
  });

  test('permissions update dynamically', function() {
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails('https://foo.com:443');

    const siteDetailsPermission =
        testElement.root.querySelector('#notifications');

    // Wait for all the permissions to be populated initially.
    return browserProxy.whenCalled('isOriginValid')
        .then(() => {
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then(() => {
          // Make sure initial state is as expected.
          assertEquals(
              settings.ContentSetting.ASK, siteDetailsPermission.site.setting);
          assertEquals(
              settings.SiteSettingSource.POLICY,
              siteDetailsPermission.site.source);
          assertEquals(
              settings.ContentSetting.ASK,
              siteDetailsPermission.$.permission.value);

          // Set new prefs and make sure only that permission is updated.
          const newException = {
            embeddingOrigin: testElement.origin,
            origin: testElement.origin,
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.DEFAULT,
          };
          browserProxy.resetResolver('getOriginPermissions');
          browserProxy.setSingleException(
              settings.ContentSettingsTypes.NOTIFICATIONS, newException);
          return browserProxy.whenCalled('getOriginPermissions');
        })
        .then((args) => {
          // The notification pref was just updated, so make sure the call to
          // getOriginPermissions was to check notifications.
          assertTrue(
              args[1].includes(settings.ContentSettingsTypes.NOTIFICATIONS));

          // Check |siteDetailsPermission| now shows the new permission value.
          assertEquals(
              settings.ContentSetting.BLOCK,
              siteDetailsPermission.site.setting);
          assertEquals(
              settings.SiteSettingSource.DEFAULT,
              siteDetailsPermission.site.source);
          assertEquals(
              settings.ContentSetting.DEFAULT,
              siteDetailsPermission.$.permission.value);
        });
  });

  test('invalid origins navigate back', function() {
    const invalid_url = 'invalid url';
    browserProxy.setIsOriginValid(false);

    settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);

    testElement = createSiteDetails(invalid_url);
    assertEquals(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
        settings.Router.getInstance().getCurrentRoute().path);
    return browserProxy.whenCalled('isOriginValid')
        .then((args) => {
          assertEquals(invalid_url, args);
          return new Promise((resolve) => {
            listenOnce(window, 'popstate', resolve);
          });
        })
        .then(() => {
          assertEquals(
              settings.routes.SITE_SETTINGS.path,
              settings.Router.getInstance().getCurrentRoute().path);
        });
  });

  test('call fetch block autoplay status', function() {
    const origin = 'https://foo.com:443';
    browserProxy.setPrefs(prefs);
    testElement = createSiteDetails(origin);
    return browserProxy.whenCalled('fetchBlockAutoplayStatus');
  });

});
