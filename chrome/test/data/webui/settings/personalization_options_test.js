// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_personalization_options', function() {
  /**
   * @param {!Element} element
   * @param {boolean} displayed
   */
  function assertVisible(element, displayed) {
    assertEquals(
        displayed, window.getComputedStyle(element)['display'] != 'none');
  }

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
        passwordsLeakDetectionEnabled: true,
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

    test('PrivacySettingsRedesignEnabled_False', function() {
      // Ensure that elements hidden by the updated privacy settings
      // flag remain visible when the flag is in the default state
      assertFalse(loadTimeData.getBoolean('privacySettingsRedesignEnabled'));
      assertVisible(testElement.$$('#safeBrowsingToggle'), true);
      assertVisible(testElement.$$('#passwordsLeakDetectionToggle'), true);
      assertVisible(testElement.$$('#safeBrowsingReportingToggle'), true);
      assertFalse(!!testElement.$$('#signinAllowedToggle'));
    });

    test('safeBrowsingReportingToggle', function() {
      const safeBrowsingToggle = testElement.$$('#safeBrowsingToggle');
      const safeBrowsingReportingToggle =
          testElement.$$('#safeBrowsingReportingToggle');
      assertTrue(safeBrowsingToggle.checked);
      assertFalse(safeBrowsingReportingToggle.disabled);
      assertTrue(safeBrowsingReportingToggle.checked);
      safeBrowsingToggle.click();
      Polymer.dom.flush();

      assertFalse(safeBrowsingToggle.checked);
      assertTrue(safeBrowsingReportingToggle.disabled);
      assertFalse(safeBrowsingReportingToggle.checked);
      assertTrue(testElement.prefs.safebrowsing.scout_reporting_enabled.value);
      safeBrowsingToggle.click();
      Polymer.dom.flush();

      assertTrue(safeBrowsingToggle.checked);
      assertFalse(safeBrowsingReportingToggle.disabled);
      assertTrue(safeBrowsingReportingToggle.checked);
    });
  });

  suite('PrivacySettingsRedesignTests', function() {
    /** @type {SettingsPrivacyPageElement} */
    let page;

    suiteSetup(function() {
      loadTimeData.overrideValues({
        privacySettingsRedesignEnabled: true,
      });
    });

    setup(function() {
      const testBrowserProxy = new TestPrivacyPageBrowserProxy();
      settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
      const syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
      PolymerTest.clearBody();

      page = document.createElement('settings-personalization-options');
      page.prefs = {
        profile: {password_manager_leak_detection: {value: true}},
        safebrowsing:
            {enabled: {value: true}, scout_reporting_enabled: {value: true}},
        signin: {
          allowed_on_next_startup:
              {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
        },
      };
      document.body.appendChild(page);
      Polymer.dom.flush();
    });

    teardown(function() {
      page.remove();
    });

    test('PrivacySettingsRedesignEnabled_True', function() {
      Polymer.dom.flush();
      assertFalse(!!page.$$('#safeBrowsingToggle'));
      assertFalse(!!page.$$('#passwordsLeakDetectionToggle'));
      assertFalse(!!page.$$('#safeBrowsingReportingToggle'));
    });

    if (!cr.isChromeOS) {
      test('signinAllowedToggle', function() {
        const toggle = page.$$('#signinAllowedToggle');
        assertVisible(toggle, true);

        page.syncStatus = {signedIn: false};
        // Check initial setup.
        assertTrue(toggle.checked);
        assertTrue(page.prefs.signin.allowed_on_next_startup.value);
        assertFalse(!!page.$.toast.open);

        // When the user is signed out, clicking the toggle should work
        // normally and the restart toast should be opened.
        toggle.click();
        assertFalse(toggle.checked);
        assertFalse(page.prefs.signin.allowed_on_next_startup.value);
        assertTrue(page.$.toast.open);

        // Clicking it again, turns the toggle back on. The toast remains
        // open.
        toggle.click();
        assertTrue(toggle.checked);
        assertTrue(page.prefs.signin.allowed_on_next_startup.value);
        assertTrue(page.$.toast.open);

        // Reset toast.
        page.showRestartToast_ = false;
        assertFalse(page.$.toast.open);

        page.syncStatus = {signedIn: true};
        // When the user is signed in, clicking the toggle should open the
        // sign-out dialog.
        assertFalse(!!page.$$('settings-signout-dialog'));
        toggle.click();
        return test_util.eventToPromise('cr-dialog-open', page)
            .then(function() {
              Polymer.dom.flush();
              // The toggle remains on.
              assertTrue(toggle.checked);
              assertTrue(page.prefs.signin.allowed_on_next_startup.value);
              assertFalse(page.$.toast.open);

              const signoutDialog = page.$$('settings-signout-dialog');
              assertTrue(!!signoutDialog);
              assertTrue(signoutDialog.$$('#dialog').open);

              // The user clicks cancel.
              const cancel = signoutDialog.$$('#disconnectCancel');
              cancel.click();

              return test_util.eventToPromise('close', signoutDialog);
            })
            .then(function() {
              Polymer.dom.flush();
              assertFalse(!!page.$$('settings-signout-dialog'));

              // After the dialog is closed, the toggle remains turned on.
              assertTrue(toggle.checked);
              assertTrue(page.prefs.signin.allowed_on_next_startup.value);
              assertFalse(page.$.toast.open);

              // The user clicks the toggle again.
              toggle.click();
              return test_util.eventToPromise('cr-dialog-open', page);
            })
            .then(function() {
              Polymer.dom.flush();
              const signoutDialog = page.$$('settings-signout-dialog');
              assertTrue(!!signoutDialog);
              assertTrue(signoutDialog.$$('#dialog').open);

              // The user clicks confirm, which signs them out.
              const disconnectConfirm = signoutDialog.$$('#disconnectConfirm');
              disconnectConfirm.click();

              return test_util.eventToPromise('close', signoutDialog);
            })
            .then(function() {
              Polymer.dom.flush();
              // After the dialog is closed, the toggle is turned off and the
              // toast is shown.
              assertFalse(toggle.checked);
              assertFalse(page.prefs.signin.allowed_on_next_startup.value);
              assertTrue(page.$.toast.open);
            });
      });
    }
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
