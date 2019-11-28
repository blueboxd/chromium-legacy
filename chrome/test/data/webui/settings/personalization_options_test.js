// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_personalization_options', function() {
  suite('PersonalizationOptionsTests_AllBuilds', function() {
    /** @type {settings.TestPrivacyPageBrowserProxy} */
    let testBrowserProxy;

    /** @type {settings.SyncBrowserProxy} */
    let syncBrowserProxy;

    /** @type {SettingsPersonalizationOptionsElement} */
    let testElement;

    suiteSetup(function() {
      loadTimeData.overrideValues({
        driveSuggestAvailable: true,
      });
    });

    setup(function() {
      testBrowserProxy = new TestPrivacyPageBrowserProxy();
      settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
      syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
      PolymerTest.clearBody();
      testElement = document.createElement('settings-personalization-options');
      testElement.prefs = {
        profile: {password_manager_leak_detection: {value: true}},
        safebrowsing:
            {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      };
      document.body.appendChild(testElement);
      Polymer.dom.flush();
    });

    teardown(function() {
      testElement.remove();
    });

    test('DriveSearchSuggestControl', function() {
      assertFalse(!!testElement.$$('#driveSuggestControl'));

      testElement.syncStatus = {
        signedIn: true,
        statusAction: settings.StatusAction.NO_ACTION
      };
      Polymer.dom.flush();
      assertTrue(!!testElement.$$('#driveSuggestControl'));

      testElement.syncStatus = {
        signedIn: true,
        statusAction: settings.StatusAction.REAUTHENTICATE
      };
      Polymer.dom.flush();
      assertFalse(!!testElement.$$('#driveSuggestControl'));
    });

    test('leakDetectionToggleSignedOutWithFalsePref', function() {
      testElement.set(
          'prefs.profile.password_manager_leak_detection.value', false);
      testElement.syncStatus = {signedIn: false};
      Polymer.dom.flush();

      assertTrue(testElement.$.passwordsLeakDetectionCheckbox.disabled);
      assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
    });

    test('leakDetectionToggleSignedOutWithTruePref', function() {
      testElement.syncStatus = {signedIn: false};
      Polymer.dom.flush();

      assertTrue(testElement.$.passwordsLeakDetectionCheckbox.disabled);
      assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals(
          loadTimeData.getString(
              'passwordsLeakDetectionSignedOutEnabledDescription'),
          testElement.$.passwordsLeakDetectionCheckbox.subLabel);
    });

    if (!cr.isChromeOS) {
      test('leakDetectionToggleSignedInNotSyncingWithFalsePref', function() {
        testElement.set(
            'prefs.profile.password_manager_leak_detection.value', false);
        testElement.syncStatus = {signedIn: false};
        sync_test_util.simulateStoredAccounts([
          {
            fullName: 'testName',
            givenName: 'test',
            email: 'test@test.com',
          },
        ]);
        Polymer.dom.flush();

        assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
        assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
        assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
      });

      test('leakDetectionToggleSignedInNotSyncingWithTruePref', function() {
        testElement.syncStatus = {signedIn: false};
        sync_test_util.simulateStoredAccounts([
          {
            fullName: 'testName',
            givenName: 'test',
            email: 'test@test.com',
          },
        ]);
        Polymer.dom.flush();

        assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
        assertTrue(testElement.$.passwordsLeakDetectionCheckbox.checked);
        assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
      });
    }

    test('leakDetectionToggleSignedInAndSyncingWithFalsePref', function() {
      testElement.set(
          'prefs.profile.password_manager_leak_detection.value', false);
      testElement.syncStatus = {signedIn: true};
      Polymer.dom.flush();

      assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
      assertFalse(testElement.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
    });

    test('leakDetectionToggleSignedInAndSyncingWithTruePref', function() {
      testElement.syncStatus = {signedIn: true};
      Polymer.dom.flush();

      assertFalse(testElement.$.passwordsLeakDetectionCheckbox.disabled);
      assertTrue(testElement.$.passwordsLeakDetectionCheckbox.checked);
      assertEquals('', testElement.$.passwordsLeakDetectionCheckbox.subLabel);
    });
  });

  suite('PersonalizationOptionsTests_OfficialBuild', function() {
    /** @type {settings.TestPrivacyPageBrowserProxy} */
    let testBrowserProxy;

    /** @type {SettingsPersonalizationOptionsElement} */
    let testElement;

    setup(function() {
      testBrowserProxy = new TestPrivacyPageBrowserProxy();
      settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
      PolymerTest.clearBody();
      testElement = document.createElement('settings-personalization-options');
      document.body.appendChild(testElement);
    });

    teardown(function() {
      testElement.remove();
    });

    test('Spellcheck toggle', function() {
      testElement.prefs = {
        profile: {password_manager_leak_detection: {value: true}},
        safebrowsing:
            {enabled: {value: true}, scout_reporting_enabled: {value: true}},
        spellcheck: {dictionaries: {value: ['en-US']}}
      };
      Polymer.dom.flush();
      assertFalse(testElement.$.spellCheckControl.hidden);

      testElement.prefs = {
        profile: {password_manager_leak_detection: {value: true}},
        safebrowsing:
            {enabled: {value: true}, scout_reporting_enabled: {value: true}},
        spellcheck: {dictionaries: {value: []}}
      };
      Polymer.dom.flush();
      assertTrue(testElement.$.spellCheckControl.hidden);

      testElement.prefs = {
        profile: {password_manager_leak_detection: {value: true}},
        safebrowsing:
            {enabled: {value: true}, scout_reporting_enabled: {value: true}},
        browser: {enable_spellchecking: {value: false}},
        spellcheck: {
          dictionaries: {value: ['en-US']},
          use_spelling_service: {value: false}
        }
      };
      Polymer.dom.flush();
      testElement.$.spellCheckControl.click();
      assertTrue(testElement.prefs.spellcheck.use_spelling_service.value);
    });
  });
});
