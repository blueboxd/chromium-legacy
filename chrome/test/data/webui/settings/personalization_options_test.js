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
        privacySettingsRedesign: false,
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
        signin: {
          allowed_on_next_startup:
              {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
        },
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

    if (!cr.isChromeOS) {
      test('signinAllowedToggle', function() {
        const toggle = testElement.$.signinAllowedToggle;
        assertVisible(toggle, true);

        testElement.syncStatus = {signedIn: false};
        // Check initial setup.
        assertTrue(toggle.checked);
        assertTrue(testElement.prefs.signin.allowed_on_next_startup.value);
        assertFalse(!!testElement.$.toast.open);

        // When the user is signed out, clicking the toggle should work
        // normally and the restart toast should be opened.
        toggle.click();
        assertFalse(toggle.checked);
        assertFalse(testElement.prefs.signin.allowed_on_next_startup.value);
        assertTrue(testElement.$.toast.open);

        // Clicking it again, turns the toggle back on. The toast remains
        // open.
        toggle.click();
        assertTrue(toggle.checked);
        assertTrue(testElement.prefs.signin.allowed_on_next_startup.value);
        assertTrue(testElement.$.toast.open);

        // Reset toast.
        testElement.showRestartToast_ = false;
        assertFalse(testElement.$.toast.open);

        testElement.syncStatus = {signedIn: true};
        // When the user is signed in, clicking the toggle should open the
        // sign-out dialog.
        assertFalse(!!testElement.$$('settings-signout-dialog'));
        toggle.click();
        return test_util.eventToPromise('cr-dialog-open', testElement)
            .then(function() {
              Polymer.dom.flush();
              // The toggle remains on.
              assertTrue(toggle.checked);
              assertTrue(
                  testElement.prefs.signin.allowed_on_next_startup.value);
              assertFalse(testElement.$.toast.open);

              const signoutDialog = testElement.$$('settings-signout-dialog');
              assertTrue(!!signoutDialog);
              assertTrue(signoutDialog.$$('#dialog').open);

              // The user clicks cancel.
              const cancel = signoutDialog.$$('#disconnectCancel');
              cancel.click();

              return test_util.eventToPromise('close', signoutDialog);
            })
            .then(function() {
              Polymer.dom.flush();
              assertFalse(!!testElement.$$('settings-signout-dialog'));

              // After the dialog is closed, the toggle remains turned on.
              assertTrue(toggle.checked);
              assertTrue(
                  testElement.prefs.signin.allowed_on_next_startup.value);
              assertFalse(testElement.$.toast.open);

              // The user clicks the toggle again.
              toggle.click();
              return test_util.eventToPromise('cr-dialog-open', testElement);
            })
            .then(function() {
              Polymer.dom.flush();
              const signoutDialog = testElement.$$('settings-signout-dialog');
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
              assertFalse(
                  testElement.prefs.signin.allowed_on_next_startup.value);
              assertTrue(testElement.$.toast.open);
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
