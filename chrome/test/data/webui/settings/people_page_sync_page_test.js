// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {Router, routes, SyncBrowserProxyImpl, PageStatus, StatusAction} from 'chrome://settings/settings.js';
// #import 'chrome://settings/lazy_load.js';
// #import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// #import {getSyncAllPrefs, setupRouterWithSyncRoutes, simulateStoredAccounts} from 'chrome://test/settings/sync_test_util.m.js';
// #import {eventToPromise, waitBeforeNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

cr.define('settings_people_page_sync_page', function() {
  suite('SyncSettingsTests', function() {
    let syncPage = null;
    let browserProxy = null;
    let encryptionElement = null;
    let encryptionRadioGroup = null;
    let encryptWithGoogle = null;
    let encryptWithPassphrase = null;

    function setupSyncPage() {
      PolymerTest.clearBody();
      syncPage = document.createElement('settings-sync-page');
      const router = settings.Router.getInstance();
      router.navigateTo(router.getRoutes().SYNC);
      // Preferences should exist for embedded
      // 'personalization_options.html'. We don't perform tests on them.
      syncPage.prefs = {
        profile: {password_manager_leak_detection: {value: true}},
        signin: {
          allowed_on_next_startup:
              {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
        },
        safebrowsing:
            {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      };

      document.body.appendChild(syncPage);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.CONFIGURE);
      assertFalse(syncPage.$$('#' + settings.PageStatus.CONFIGURE).hidden);
      assertTrue(syncPage.$$('#' + settings.PageStatus.SPINNER).hidden);

      // Start with Sync All with no encryption selected. Also, ensure
      // that this is not a supervised user, so that Sync Passphrase is
      // enabled.
      cr.webUIListenerCallback(
          'sync-prefs-changed', sync_test_util.getSyncAllPrefs());
      syncPage.set('syncStatus', {supervisedUser: false});
      Polymer.dom.flush();
    }

    suiteSetup(function() {
      loadTimeData.overrideValues({
        syncSetupFriendlySettings: true,
        signinAllowed: true
      });
    });

    setup(function() {
      sync_test_util.setupRouterWithSyncRoutes();
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      setupSyncPage();

      return test_util.waitBeforeNextRender().then(() => {
        encryptionElement = syncPage.$$('settings-sync-encryption-options');
        assertTrue(!!encryptionElement);
        encryptionRadioGroup = encryptionElement.$$('#encryptionRadioGroup');
        encryptWithGoogle =
            encryptionElement.$$('cr-radio-button[name="encrypt-with-google"]');
        encryptWithPassphrase = encryptionElement.$$(
            'cr-radio-button[name="encrypt-with-passphrase"]');
        assertTrue(!!encryptionRadioGroup);
        assertTrue(!!encryptWithGoogle);
        assertTrue(!!encryptWithPassphrase);
      });
    });

    teardown(function() {
      syncPage.remove();
    });

    // #######################
    // TESTS FOR ALL PLATFORMS
    // #######################

    test('NotifiesHandlerOfNavigation', function() {
      const router = settings.Router.getInstance();
      function testNavigateAway() {
        router.navigateTo(router.getRoutes().PEOPLE);
        return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
      }

      function testNavigateBack() {
        browserProxy.resetResolver('didNavigateToSyncPage');
        router.navigateTo(router.getRoutes().SYNC);
        return browserProxy.whenCalled('didNavigateToSyncPage');
      }

      function testDetach() {
        browserProxy.resetResolver('didNavigateAwayFromSyncPage');
        syncPage.remove();
        return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
      }

      function testRecreate() {
        browserProxy.resetResolver('didNavigateToSyncPage');
        syncPage = document.createElement('settings-sync-page');
        router.navigateTo(router.getRoutes().SYNC);

        document.body.appendChild(syncPage);
        return browserProxy.whenCalled('didNavigateToSyncPage');
      }

      return browserProxy.whenCalled('didNavigateToSyncPage')
          .then(testNavigateAway)
          .then(testNavigateBack)
          .then(testDetach)
          .then(testRecreate);
    });

    test('SyncSectionLayout_SignedIn', function() {
      const syncSection = syncPage.$$('#sync-section');
      const otherItems = syncPage.$$('#other-sync-items');

      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      Polymer.dom.flush();
      assertFalse(syncSection.hidden);
      assertTrue(syncPage.$$('#sync-separator').hidden);
      assertTrue(otherItems.classList.contains('list-frame'));
      assertEquals(
          otherItems.querySelectorAll(':scope > .list-item').length, 1);
      assertEquals(
          otherItems.querySelectorAll(':scope > cr-link-row').length, 3);

      // Test sync paused state.
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: true,
        statusAction: settings.StatusAction.REAUTHENTICATE
      };
      assertTrue(syncSection.hidden);
      assertFalse(syncPage.$$('#sync-separator').hidden);

      // Test passphrase error state.
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: true,
        statusAction: settings.StatusAction.ENTER_PASSPHRASE
      };
      assertFalse(syncSection.hidden);
      assertTrue(syncPage.$$('#sync-separator').hidden);
    });

    test('SyncSectionLayout_SignedOut', function() {
      const syncSection = syncPage.$$('#sync-section');

      syncPage.syncStatus = {
        signedIn: false,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      Polymer.dom.flush();
      assertTrue(syncSection.hidden);
      assertFalse(syncPage.$$('#sync-separator').hidden);
    });

    test('SyncSectionLayout_SyncDisabled', function() {
      const syncSection = syncPage.$$('#sync-section');

      syncPage.syncStatus = {
        signedIn: false,
        disabled: true,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      Polymer.dom.flush();
      assertTrue(syncSection.hidden);
    });

    test('LoadingAndTimeout', function() {
      const configurePage = syncPage.$$('#' + settings.PageStatus.CONFIGURE);
      const spinnerPage = syncPage.$$('#' + settings.PageStatus.SPINNER);

      // NOTE: This isn't called in production, but the test suite starts the
      // tests with PageStatus.CONFIGURE.
      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.SPINNER);
      assertTrue(configurePage.hidden);
      assertFalse(spinnerPage.hidden);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.CONFIGURE);
      assertFalse(configurePage.hidden);
      assertTrue(spinnerPage.hidden);

      // Should remain on the CONFIGURE page even if the passphrase failed.
      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.PASSPHRASE_FAILED);
      assertFalse(configurePage.hidden);
      assertTrue(spinnerPage.hidden);
    });

    test('EncryptionExpandButton', function() {
      const encryptionDescription = syncPage.$$('#encryptionDescription');
      const encryptionCollapse = syncPage.$$('#encryptionCollapse');

      // No encryption with custom passphrase.
      assertFalse(encryptionCollapse.opened);
      encryptionDescription.click();
      assertTrue(encryptionCollapse.opened);

      encryptionDescription.click();
      assertFalse(encryptionCollapse.opened);

      // Data encrypted with custom passphrase.
      // The encryption menu should be expanded.
      const prefs = sync_test_util.getSyncAllPrefs();
      prefs.encryptAllData = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs);
      Polymer.dom.flush();
      assertTrue(encryptionCollapse.opened);

      // Clicking |reset Sync| does not change the expansion state.
      const link = encryptionDescription.querySelector('a[href]');
      assertTrue(!!link);
      link.target = '';
      link.href = '#';
      // Prevent the link from triggering a page navigation when tapped.
      // Breaks the test in Vulcanized mode.
      link.addEventListener('click', e => e.preventDefault());
      link.click();
      assertTrue(encryptionCollapse.opened);
    });

    test('RadioBoxesEnabledWhenUnencrypted', function() {
      // Verify that the encryption radio boxes are enabled.
      assertFalse(encryptionRadioGroup.disabled);
      assertEquals(encryptWithGoogle.ariaDisabled, 'false');
      assertEquals(encryptWithPassphrase.ariaDisabled, 'false');

      assertTrue(encryptWithGoogle.checked);

      // Select 'Encrypt with passphrase' to create a new passphrase.
      assertFalse(!!encryptionElement.$$('#create-password-box'));

      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!encryptionElement.$$('#create-password-box'));
      const saveNewPassphrase = encryptionElement.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      // Test that a sync prefs update does not reset the selection.
      cr.webUIListenerCallback(
          'sync-prefs-changed', sync_test_util.getSyncAllPrefs());
      Polymer.dom.flush();
      assertTrue(encryptWithPassphrase.checked);
    });

    test('ClickingLinkDoesNotChangeRadioValue', function() {
      assertFalse(encryptionRadioGroup.disabled);
      assertEquals(encryptWithPassphrase.ariaDisabled, 'false');
      assertFalse(encryptWithPassphrase.checked);

      const link = encryptWithPassphrase.querySelector('a[href]');
      assertTrue(!!link);

      // Suppress opening a new tab, since then the test will continue running
      // on a background tab (which has throttled timers) and will timeout.
      link.target = '';
      link.href = '#';
      // Prevent the link from triggering a page navigation when tapped.
      // Breaks the test in Vulcanized mode.
      link.addEventListener('click', function(e) {
        e.preventDefault();
      });

      link.click();

      assertFalse(encryptWithPassphrase.checked);
    });

    test('SaveButtonDisabledWhenPassphraseOrConfirmationEmpty', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!encryptionElement.$$('#create-password-box'));
      const saveNewPassphrase = encryptionElement.$$('#saveNewPassphrase');
      const passphraseInput = encryptionElement.$$('#passphraseInput');
      const passphraseConfirmationInput =
          encryptionElement.$$('#passphraseConfirmationInput');

      passphraseInput.value = '';
      passphraseConfirmationInput.value = '';
      assertTrue(saveNewPassphrase.disabled);

      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = '';
      assertTrue(saveNewPassphrase.disabled);

      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'bar';
      assertFalse(saveNewPassphrase.disabled);
    });

    test('CreatingPassphraseMismatchedPassphrase', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!encryptionElement.$$('#create-password-box'));
      const saveNewPassphrase = encryptionElement.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      const passphraseInput = encryptionElement.$$('#passphraseInput');
      const passphraseConfirmationInput =
          encryptionElement.$$('#passphraseConfirmationInput');
      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'bar';

      saveNewPassphrase.click();
      Polymer.dom.flush();

      assertFalse(passphraseInput.invalid);
      assertTrue(passphraseConfirmationInput.invalid);

      assertFalse(syncPage.syncPrefs.encryptAllData);
    });

    test('CreatingPassphraseValidPassphrase', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!encryptionElement.$$('#create-password-box'));
      const saveNewPassphrase = encryptionElement.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      const passphraseInput = encryptionElement.$$('#passphraseInput');
      const passphraseConfirmationInput =
          encryptionElement.$$('#passphraseConfirmationInput');
      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'foo';
      saveNewPassphrase.click();

      function verifyPrefs(prefs) {
        const expected = sync_test_util.getSyncAllPrefs();
        expected.setNewPassphrase = true;
        expected.passphrase = 'foo';
        expected.encryptAllData = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        expected.fullEncryptionBody = 'Encrypted with custom passphrase';
        cr.webUIListenerCallback('sync-prefs-changed', expected);

        Polymer.dom.flush();

        return test_util.waitBeforeNextRender(syncPage).then(() => {
          // Need to re-retrieve this, as a different show passphrase radio
          // button is shown once |syncPrefs.fullEncryptionBody| is non-empty.
          encryptWithPassphrase = encryptionElement.$$(
              'cr-radio-button[name="encrypt-with-passphrase"]');

          // Assert that the radio boxes are disabled after encryption enabled.
          assertTrue(encryptionRadioGroup.disabled);
          assertEquals(-1, encryptWithGoogle.$.button.tabIndex);
          assertEquals(-1, encryptWithPassphrase.$.button.tabIndex);
        });
      }
      return browserProxy.whenCalled('setSyncEncryption').then(verifyPrefs);
    });

    test('RadioBoxesHiddenWhenPassphraseRequired', function() {
      const prefs = sync_test_util.getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      prefs.fullEncryptionBody = 'Sync already encrypted.';
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      Polymer.dom.flush();

      assertTrue(syncPage.$$('#encryptionDescription').hidden);
      assertEquals(
          encryptionElement.$$('#encryptionRadioGroupContainer').style.display,
          'none');
    });

    test(
        'ExistingPassphraseSubmitButtonDisabledWhenExistingPassphraseEmpty',
        function() {
          const prefs = sync_test_util.getSyncAllPrefs();
          prefs.encryptAllData = true;
          prefs.passphraseRequired = true;
          cr.webUIListenerCallback('sync-prefs-changed', prefs);
          Polymer.dom.flush();

          const existingPassphraseInput =
              syncPage.$$('#existingPassphraseInput');
          const submitExistingPassphrase =
              syncPage.$$('#submitExistingPassphrase');

          existingPassphraseInput.value = '';
          assertTrue(submitExistingPassphrase.disabled);

          existingPassphraseInput.value = 'foo';
          assertFalse(submitExistingPassphrase.disabled);
        });

    test('EnterExistingWrongPassphrase', function() {
      const prefs = sync_test_util.getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs);
      Polymer.dom.flush();

      const existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
      assertTrue(!!existingPassphraseInput);
      existingPassphraseInput.value = 'wrong';
      browserProxy.encryptionResponse = settings.PageStatus.PASSPHRASE_FAILED;

      const submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');
      assertTrue(!!submitExistingPassphrase);
      submitExistingPassphrase.click();

      return browserProxy.whenCalled('setSyncEncryption').then(function(prefs) {
        const expected = sync_test_util.getSyncAllPrefs();
        expected.setNewPassphrase = false;
        expected.passphrase = 'wrong';
        expected.encryptAllData = true;
        expected.passphraseRequired = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        Polymer.dom.flush();

        assertTrue(existingPassphraseInput.invalid);
      });
    });

    test('EnterExistingCorrectPassphrase', function() {
      const prefs = sync_test_util.getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs);
      Polymer.dom.flush();

      const existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
      assertTrue(!!existingPassphraseInput);
      existingPassphraseInput.value = 'right';
      browserProxy.encryptionResponse = settings.PageStatus.CONFIGURE;

      const submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');
      assertTrue(!!submitExistingPassphrase);
      submitExistingPassphrase.click();

      return browserProxy.whenCalled('setSyncEncryption').then(function(prefs) {
        const expected = sync_test_util.getSyncAllPrefs();
        expected.setNewPassphrase = false;
        expected.passphrase = 'right';
        expected.encryptAllData = true;
        expected.passphraseRequired = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        const newPrefs = sync_test_util.getSyncAllPrefs();
        newPrefs.encryptAllData = true;
        cr.webUIListenerCallback('sync-prefs-changed', newPrefs);

        Polymer.dom.flush();

        // Verify that the encryption radio boxes are shown but disabled.
        assertTrue(encryptionRadioGroup.disabled);
        assertEquals(-1, encryptWithGoogle.$.button.tabIndex);
        assertEquals(-1, encryptWithPassphrase.$.button.tabIndex);
      });
    });

    test('SyncAdvancedRow', function() {
      Polymer.dom.flush();

      const syncAdvancedRow = syncPage.$$('#sync-advanced-row');
      assertFalse(syncAdvancedRow.hidden);

      syncAdvancedRow.click();
      Polymer.dom.flush();

      assertEquals(
          settings.routes.SYNC_ADVANCED.path,
          settings.Router.getInstance().getCurrentRoute().path);
    });

    // This test checks whether the passphrase encryption options are
    // disabled. This is important for supervised accounts. Because sync
    // is required for supervision, passphrases should remain disabled.
    test('DisablingSyncPassphrase', function() {
      // We initialize a new SyncPrefs object for each case, because
      // otherwise the webUIListener doesn't update.

      // 1) Normal user (full data encryption allowed)
      // EXPECTED: encryptionOptions enabled
      const prefs1 = sync_test_util.getSyncAllPrefs();
      prefs1.encryptAllDataAllowed = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs1);
      syncPage.syncStatus = {supervisedUser: false};
      Polymer.dom.flush();
      assertFalse(encryptionRadioGroup.disabled);
      assertEquals(encryptWithGoogle.ariaDisabled, 'false');
      assertEquals(encryptWithPassphrase.ariaDisabled, 'false');

      // 2) Normal user (full data encryption not allowed)
      // encryptAllDataAllowed is usually false only for supervised
      // users, but it's better to be check this case.
      // EXPECTED: encryptionOptions disabled
      const prefs2 = sync_test_util.getSyncAllPrefs();
      prefs2.encryptAllDataAllowed = false;
      cr.webUIListenerCallback('sync-prefs-changed', prefs2);
      syncPage.syncStatus = {supervisedUser: false};
      Polymer.dom.flush();
      assertTrue(encryptionRadioGroup.disabled);
      assertEquals(encryptWithGoogle.ariaDisabled, 'true');
      assertEquals(encryptWithPassphrase.ariaDisabled, 'true');

      // 3) Supervised user (full data encryption not allowed)
      // EXPECTED: encryptionOptions disabled
      const prefs3 = sync_test_util.getSyncAllPrefs();
      prefs3.encryptAllDataAllowed = false;
      cr.webUIListenerCallback('sync-prefs-changed', prefs3);
      syncPage.syncStatus = {supervisedUser: true};
      Polymer.dom.flush();
      assertTrue(encryptionRadioGroup.disabled);
      assertEquals(encryptWithGoogle.ariaDisabled, 'true');
      assertEquals(encryptWithPassphrase.ariaDisabled, 'true');

      // 4) Supervised user (full data encryption allowed)
      // This never happens in practice, but just to be safe.
      // EXPECTED: encryptionOptions disabled
      const prefs4 = sync_test_util.getSyncAllPrefs();
      prefs4.encryptAllDataAllowed = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs4);
      syncPage.syncStatus = {supervisedUser: true};
      Polymer.dom.flush();
      assertTrue(encryptionRadioGroup.disabled);
      assertEquals(encryptWithGoogle.ariaDisabled, 'true');
      assertEquals(encryptWithPassphrase.ariaDisabled, 'true');
    });

    // The sync dashboard is not accessible by supervised
    // users, so it should remain hidden.
    test('SyncDashboardHiddenFromSupervisedUsers', function() {
      const dashboardLink = syncPage.$$('#syncDashboardLink');

      const prefs = sync_test_util.getSyncAllPrefs();
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      // Normal user
      syncPage.syncStatus = {supervisedUser: false};
      Polymer.dom.flush();
      assertFalse(dashboardLink.hidden);

      // Supervised user
      syncPage.syncStatus = {supervisedUser: true};
      Polymer.dom.flush();
      assertTrue(dashboardLink.hidden);
    });

    // ##################################
    // TESTS THAT ARE SKIPPED ON CHROMEOS
    // ##################################

    if (!cr.isChromeOS) {
      test('SyncSetupCancel', function() {
        syncPage.syncStatus = {
          syncSystemEnabled: true,
          firstSetupInProgress: true,
          signedIn: true
        };
        Polymer.dom.flush();
        sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);

        const cancelButton =
            syncPage.$$('settings-sync-account-control')
                .shadowRoot.querySelector('#setup-buttons cr-button');
        assertTrue(!!cancelButton);

        // Clicking the setup cancel button aborts sync.
        cancelButton.click();
        return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
            .then(abort => {
              assertTrue(abort);
            });
      });

      test('SyncSetupConfirm', function() {
        syncPage.syncStatus = {
          syncSystemEnabled: true,
          firstSetupInProgress: true,
          signedIn: true
        };
        Polymer.dom.flush();
        sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);

        const confirmButton =
            syncPage.$$('settings-sync-account-control')
                .shadowRoot.querySelector('#setup-buttons .action-button');

        assertTrue(!!confirmButton);
        confirmButton.click();

        return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
            .then(abort => {
              assertFalse(abort);
            });
      });

      test('SyncSetupLeavePage', function() {
        syncPage.syncStatus = {
          syncSystemEnabled: true,
          firstSetupInProgress: true,
          signedIn: true
        };
        Polymer.dom.flush();

        // Navigating away while setup is in progress opens the 'Cancel sync?'
        // dialog.
        const router = settings.Router.getInstance();
        router.navigateTo(settings.routes.BASIC);
        return test_util.eventToPromise('cr-dialog-open', syncPage)
            .then(() => {
              assertEquals(router.getRoutes().SYNC, router.getCurrentRoute());
              assertTrue(syncPage.$$('#setupCancelDialog').open);

              // Clicking the cancel button on the 'Cancel sync?' dialog closes
              // the dialog and removes it from the DOM.
              syncPage.$$('#setupCancelDialog')
                  .querySelector('.cancel-button')
                  .click();

              return test_util.eventToPromise(
                  'close', syncPage.$$('#setupCancelDialog'));
            })
            .then(() => {
              Polymer.dom.flush();
              assertEquals(router.getRoutes().SYNC, router.getCurrentRoute());
              assertFalse(!!syncPage.$$('#setupCancelDialog'));

              // Navigating away while setup is in progress opens the
              // dialog again.
              router.navigateTo(settings.routes.BASIC);
              return test_util.eventToPromise('cr-dialog-open', syncPage);
            })
            .then(() => {
              assertTrue(syncPage.$$('#setupCancelDialog').open);

              // Clicking the confirm button on the dialog aborts sync.
              syncPage.$$('#setupCancelDialog')
                  .querySelector('.action-button')
                  .click();
              return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
            })
            .then(abort => {
              assertTrue(abort);
            });
      });

      test('SyncSetupSearchSettings', function() {
        syncPage.syncStatus = {
          syncSystemEnabled: true,
          firstSetupInProgress: true,
          signedIn: true
        };
        Polymer.dom.flush();

        // Searching settings while setup is in progress cancels sync.
        const router = settings.Router.getInstance();
        router.navigateTo(
            router.getRoutes().BASIC, new URLSearchParams('search=foo'));

        return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
            .then(abort => {
              assertTrue(abort);
            });
      });

      test('ShowAccountRow', function() {
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.syncStatus = {syncSystemEnabled: false};
        Polymer.dom.flush();
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.syncStatus = {syncSystemEnabled: true};
        Polymer.dom.flush();
        assertTrue(!!syncPage.$$('settings-sync-account-control'));
      });

      test('ShowAccountRow_SigninAllowedFalse', function() {
        loadTimeData.overrideValues({signinAllowed: false});
        setupSyncPage();

        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.syncStatus = {syncSystemEnabled: false};
        Polymer.dom.flush();
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.syncStatus = {syncSystemEnabled: true};
        Polymer.dom.flush();
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
      });
    }
  });
  // #cr_define_end
});
