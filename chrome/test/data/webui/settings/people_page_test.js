// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page', function() {
  /** @implements {settings.PeopleBrowserProxy} */
  class TestPeopleBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'openURL',
      ]);
    }

    /** @override */
    openURL(url) {
      this.methodCalled('openURL', url);
    }
  }

  /** @type {?SettingsPeoplePageElement} */
  let peoplePage = null;
  /** @type {?settings.ProfileInfoBrowserProxy} */
  let profileInfoBrowserProxy = null;
  /** @type {?settings.SyncBrowserProxy} */
  let syncBrowserProxy = null;

  suite('ProfileInfoTests', function() {
    suiteSetup(function() {
      loadTimeData.overrideValues({
        // Force Dice off. Dice is tested in the DiceUITest suite.
        diceEnabled: false,
      });
      if (cr.isChromeOS) {
        loadTimeData.overrideValues({
          // Account Manager is tested in the Chrome OS-specific section below.
          isAccountManagerEnabled: false,
        });
      }
    });

    setup(async function() {
      profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
      settings.ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

      syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

      PolymerTest.clearBody();
      peoplePage = document.createElement('settings-people-page');
      peoplePage.pageVisibility = settings.pageVisibility;
      document.body.appendChild(peoplePage);

      await syncBrowserProxy.whenCalled('getSyncStatus');
      await profileInfoBrowserProxy.whenCalled('getProfileInfo');
      Polymer.dom.flush();
    });

    teardown(function() {
      peoplePage.remove();
    });

    test('GetProfileInfo', function() {
      assertEquals(
          profileInfoBrowserProxy.fakeProfileInfo.name,
          peoplePage.$$('#profile-name').textContent.trim());
      const bg = peoplePage.$$('#profile-icon').style.backgroundImage;
      assertTrue(bg.includes(profileInfoBrowserProxy.fakeProfileInfo.iconUrl));

      const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
          'LAAAAAABAAEAAAICTAEAOw==';
      cr.webUIListenerCallback(
          'profile-info-changed', {name: 'pushedName', iconUrl: iconDataUrl});

      Polymer.dom.flush();
      assertEquals(
          'pushedName', peoplePage.$$('#profile-name').textContent.trim());
      const newBg = peoplePage.$$('#profile-icon').style.backgroundImage;
      assertTrue(newBg.includes(iconDataUrl));
    });
  });

  if (!cr.isChromeOS) {
    suite('SyncStatusTests', function() {
      setup(function() {
        syncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

        profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ =
            profileInfoBrowserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        peoplePage.pageVisibility = settings.pageVisibility;
        document.body.appendChild(peoplePage);
      });

      teardown(function() {
        peoplePage.remove();
      });

      test('Toast', function() {
        assertFalse(peoplePage.$.toast.open);
        cr.webUIListenerCallback('sync-settings-saved');
        assertTrue(peoplePage.$.toast.open);
      });

      // This makes sure UI meant for DICE-enabled profiles are not leaked to
      // non-dice profiles.
      // TODO(tangltom): This should be removed once all profiles are fully
      // migrated.
      test('NoManageProfileRow', function() {
        assertFalse(!!peoplePage.$$('#edit-profile'));
      });

      test('GetProfileInfo', function() {
        let disconnectButton = null;
        return syncBrowserProxy.whenCalled('getSyncStatus')
            .then(function() {
              Polymer.dom.flush();
              disconnectButton = peoplePage.$$('#disconnectButton');
              assertTrue(!!disconnectButton);
              assertFalse(!!peoplePage.$$('settings-signout-dialog'));

              disconnectButton.click();
              Polymer.dom.flush();
            })
            .then(function() {
              const signoutDialog = peoplePage.$$('settings-signout-dialog');
              assertTrue(signoutDialog.$$('#dialog').open);
              assertFalse(signoutDialog.$$('#deleteProfile').hidden);

              const deleteProfileCheckbox = signoutDialog.$$('#deleteProfile');
              assertTrue(!!deleteProfileCheckbox);
              assertLT(0, deleteProfileCheckbox.clientHeight);

              const disconnectConfirm = signoutDialog.$$('#disconnectConfirm');
              assertTrue(!!disconnectConfirm);
              assertFalse(disconnectConfirm.hidden);

              const popstatePromise = new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });

              disconnectConfirm.click();

              return popstatePromise;
            })
            .then(function() {
              return syncBrowserProxy.whenCalled('signOut');
            })
            .then(function(deleteProfile) {
              assertFalse(deleteProfile);

              sync_test_util.simulateSyncStatus({
                signedIn: true,
                domain: 'example.com',
              });

              assertFalse(!!peoplePage.$$('#dialog'));
              disconnectButton.click();
              Polymer.dom.flush();

              return new Promise(function(resolve) {
                peoplePage.async(resolve);
              });
            })
            .then(function() {
              const signoutDialog = peoplePage.$$('settings-signout-dialog');
              assertTrue(signoutDialog.$$('#dialog').open);
              assertFalse(!!signoutDialog.$$('#deleteProfile'));

              const disconnectManagedProfileConfirm =
                  signoutDialog.$$('#disconnectManagedProfileConfirm');
              assertTrue(!!disconnectManagedProfileConfirm);
              assertFalse(disconnectManagedProfileConfirm.hidden);

              syncBrowserProxy.resetResolver('signOut');

              const popstatePromise = new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });

              disconnectManagedProfileConfirm.click();

              return popstatePromise;
            })
            .then(function() {
              return syncBrowserProxy.whenCalled('signOut');
            })
            .then(function(deleteProfile) {
              assertTrue(deleteProfile);
            });
      });

      test('getProfileStatsCount', function() {
        return syncBrowserProxy.whenCalled('getSyncStatus')
            .then(function() {
              Polymer.dom.flush();

              // Open the disconnect dialog.
              disconnectButton = peoplePage.$$('#disconnectButton');
              assertTrue(!!disconnectButton);
              disconnectButton.click();

              return profileInfoBrowserProxy.whenCalled('getProfileStatsCount');
            })
            .then(function() {
              Polymer.dom.flush();
              const signoutDialog = peoplePage.$$('settings-signout-dialog');
              assertTrue(signoutDialog.$$('#dialog').open);

              // Assert the warning message is as expected.
              const warningMessage =
                  signoutDialog.$$('.delete-profile-warning');

              cr.webUIListenerCallback('profile-stats-count-ready', 0);
              assertEquals(
                  loadTimeData.getStringF(
                      'deleteProfileWarningWithoutCounts', 'fakeUsername'),
                  warningMessage.textContent.trim());

              cr.webUIListenerCallback('profile-stats-count-ready', 1);
              assertEquals(
                  loadTimeData.getStringF(
                      'deleteProfileWarningWithCountsSingular', 'fakeUsername'),
                  warningMessage.textContent.trim());

              cr.webUIListenerCallback('profile-stats-count-ready', 2);
              assertEquals(
                  loadTimeData.getStringF(
                      'deleteProfileWarningWithCountsPlural', 2,
                      'fakeUsername'),
                  warningMessage.textContent.trim());

              // Close the disconnect dialog.
              signoutDialog.$$('#disconnectConfirm').click();
              return new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });
            });
      });

      test('NavigateDirectlyToSignOutURL', function() {
        // Navigate to chrome://settings/signOut
        settings.Router.getInstance().navigateTo(settings.routes.SIGN_OUT);

        return new Promise(function(resolve) {
                 peoplePage.async(resolve);
               })
            .then(function() {
              assertTrue(
                  peoplePage.$$('settings-signout-dialog').$$('#dialog').open);
              return profileInfoBrowserProxy.whenCalled('getProfileStatsCount');
            })
            .then(function() {
              // 'getProfileStatsCount' can be the first message sent to the
              // handler if the user navigates directly to
              // chrome://settings/signOut. if so, it should not cause a crash.
              new settings.ProfileInfoBrowserProxyImpl().getProfileStatsCount();

              // Close the disconnect dialog.
              peoplePage.$$('settings-signout-dialog')
                  .$$('#disconnectConfirm')
                  .click();
            })
            .then(function() {
              return new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });
            });
      });

      test('Signout dialog suppressed when not signed in', function() {
        return syncBrowserProxy.whenCalled('getSyncStatus')
            .then(function() {
              settings.Router.getInstance().navigateTo(
                  settings.routes.SIGN_OUT);
              return new Promise(function(resolve) {
                peoplePage.async(resolve);
              });
            })
            .then(function() {
              assertTrue(
                  peoplePage.$$('settings-signout-dialog').$$('#dialog').open);

              const popstatePromise = new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });

              sync_test_util.simulateSyncStatus({
                signedIn: false,
              });

              return popstatePromise;
            })
            .then(function() {
              const popstatePromise = new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });

              settings.Router.getInstance().navigateTo(
                  settings.routes.SIGN_OUT);

              return popstatePromise;
            });
      });
    });

    suite('DiceUITest', function() {
      suiteSetup(function() {
        // Force UIs to think DICE is enabled for this profile.
        loadTimeData.overrideValues({
          diceEnabled: true,
        });
      });

      setup(function() {
        syncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

        profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ =
            profileInfoBrowserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        peoplePage.pageVisibility = settings.pageVisibility;
        document.body.appendChild(peoplePage);

        Polymer.dom.flush();
      });

      teardown(function() {
        peoplePage.remove();
      });

      test('ShowCorrectRows', function() {
        return syncBrowserProxy.whenCalled('getSyncStatus').then(function() {
          // The correct /manageProfile link row is shown.
          assertTrue(!!peoplePage.$$('#edit-profile'));
          assertFalse(!!peoplePage.$$('#picture-subpage-trigger'));

          // Sync-overview row should not exist when diceEnabled is true, even
          // if syncStatus values would've warranted the row otherwise.
          sync_test_util.simulateSyncStatus({
            signedIn: false,
            signinAllowed: true,
            syncSystemEnabled: true,
          });
          assertFalse(!!peoplePage.$$('#sync-overview'));

          // The control element should exist when policy allows.
          const accountControl = peoplePage.$$('settings-sync-account-control');
          assertTrue(
              window.getComputedStyle(accountControl)['display'] != 'none');

          // Control element doesn't exist when policy forbids sync or sign-in.
          sync_test_util.simulateSyncStatus({
            signinAllowed: false,
            syncSystemEnabled: true,
          });
          assertEquals(
              'none', window.getComputedStyle(accountControl)['display']);

          sync_test_util.simulateSyncStatus({
            signinAllowed: true,
            syncSystemEnabled: false,
          });
          assertEquals(
              'none', window.getComputedStyle(accountControl)['display']);

          const manageGoogleAccount = peoplePage.$$('#manage-google-account');

          // Do not show Google Account when stored accounts or sync status
          // could not be retrieved.
          sync_test_util.simulateStoredAccounts(undefined);
          sync_test_util.simulateSyncStatus(undefined);
          assertEquals(
              'none', window.getComputedStyle(manageGoogleAccount)['display']);

          sync_test_util.simulateStoredAccounts([]);
          sync_test_util.simulateSyncStatus(undefined);
          assertEquals(
              'none', window.getComputedStyle(manageGoogleAccount)['display']);

          sync_test_util.simulateStoredAccounts(undefined);
          sync_test_util.simulateSyncStatus({});
          assertEquals(
              'none', window.getComputedStyle(manageGoogleAccount)['display']);

          sync_test_util.simulateStoredAccounts([]);
          sync_test_util.simulateSyncStatus({});
          assertEquals(
              'none', window.getComputedStyle(manageGoogleAccount)['display']);

          // A stored account with sync off but no error should result in the
          // Google Account being shown.
          sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
          sync_test_util.simulateSyncStatus({
            signedIn: false,
            hasError: false,
          });
          assertTrue(
              window.getComputedStyle(manageGoogleAccount)['display'] !=
              'none');

          // A stored account with sync off and error should not result in the
          // Google Account being shown.
          sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
          sync_test_util.simulateSyncStatus({
            signedIn: false,
            hasError: true,
          });
          assertEquals(
              'none', window.getComputedStyle(manageGoogleAccount)['display']);

          // A stored account with sync on but no error should result in the
          // Google Account being shown.
          sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
          sync_test_util.simulateSyncStatus({
            signedIn: true,
            hasError: false,
          });
          assertTrue(
              window.getComputedStyle(manageGoogleAccount)['display'] !=
              'none');

          // A stored account with sync on but with error should not result in
          // the Google Account being shown.
          sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
          sync_test_util.simulateSyncStatus({
            signedIn: true,
            hasError: true,
          });
          assertEquals(
              'none', window.getComputedStyle(manageGoogleAccount)['display']);
        });
      });
    });
  }

  suite('SyncSettings', function() {
    setup(async function() {
      syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

      profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
      settings.ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

      PolymerTest.clearBody();
      peoplePage = document.createElement('settings-people-page');
      peoplePage.pageVisibility = settings.pageVisibility;
      document.body.appendChild(peoplePage);

      await syncBrowserProxy.whenCalled('getSyncStatus');
      Polymer.dom.flush();
    });

    teardown(function() {
      peoplePage.remove();
    });

    test('ShowCorrectSyncRow', function() {
      assertTrue(!!peoplePage.$$('#sync-setup'));
      assertFalse(!!peoplePage.$$('#sync-status'));

      // Make sures the subpage opens even when logged out or has errors.
      sync_test_util.simulateSyncStatus({
        signedIn: false,
        statusAction: settings.StatusAction.REAUTHENTICATE,
      });

      peoplePage.$$('#sync-setup').click();
      Polymer.dom.flush();

      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.routes.SYNC);
    });
  });

  if (cr.isChromeOS) {
    /** @implements {settings.AccountManagerBrowserProxy} */
    class TestAccountManagerBrowserProxy extends TestBrowserProxy {
      constructor() {
        super([
          'getAccounts',
          'addAccount',
          'reauthenticateAccount',
          'removeAccount',
          'showWelcomeDialogIfRequired',
        ]);
      }

      /** @override */
      getAccounts() {
        this.methodCalled('getAccounts');
        return Promise.resolve([{
          id: '123',
          accountType: 1,
          isDeviceAccount: false,
          isSignedIn: true,
          unmigrated: false,
          fullName: 'Primary Account',
          email: 'user@gmail.com',
          pic: 'data:image/png;base64,primaryAccountPicData',
        }]);
      }

      /** @override */
      addAccount() {
        this.methodCalled('addAccount');
      }

      /** @override */
      reauthenticateAccount(account_email) {
        this.methodCalled('reauthenticateAccount', account_email);
      }

      /** @override */
      removeAccount(account) {
        this.methodCalled('removeAccount', account);
      }

      /** @override */
      showWelcomeDialogIfRequired() {
        this.methodCalled('showWelcomeDialogIfRequired');
      }
    }

    /** @type {?settings.AccountManagerBrowserProxy} */
    let accountManagerBrowserProxy = null;

    // Preferences should exist for embedded 'personalization_options.html'.
    // We don't perform tests on them.
    const DEFAULT_PREFS = {
      profile: {password_manager_leak_detection: {value: true}},
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
      },
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
    };

    suite('Chrome OS', function() {
      suiteSetup(function() {
        loadTimeData.overrideValues({
          // Simulate SplitSettings (OS settings in their own surface).
          showOSSettings: false,
          // Simulate ChromeOSAccountManager (Google Accounts support).
          isAccountManagerEnabled: true,
        });
      });

      setup(async function() {
        syncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

        profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ =
            profileInfoBrowserProxy;

        accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
        settings.AccountManagerBrowserProxyImpl.instance_ =
            accountManagerBrowserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        peoplePage.prefs = DEFAULT_PREFS;
        peoplePage.pageVisibility = settings.pageVisibility;
        document.body.appendChild(peoplePage);

        await accountManagerBrowserProxy.whenCalled('getAccounts');
        await syncBrowserProxy.whenCalled('getSyncStatus');
        Polymer.dom.flush();
      });

      teardown(function() {
        peoplePage.remove();
      });

      test('GAIA name and picture', async () => {
        chai.assert.include(
            peoplePage.$$('#profile-icon').style.backgroundImage,
            'data:image/png;base64,primaryAccountPicData');
        assertEquals(
            'Primary Account',
            peoplePage.$$('#profile-name').textContent.trim());
      });

      test('profile row is actionable', () => {
        // Simulate a signed-in user.
        sync_test_util.simulateSyncStatus({
          signedIn: true,
        });

        // Profile row opens account manager, so the row is actionable.
        const profileIcon = assert(peoplePage.$$('#profile-icon'));
        assertTrue(profileIcon.hasAttribute('actionable'));
        const profileRow = assert(peoplePage.$$('#profile-row'));
        assertTrue(profileRow.hasAttribute('actionable'));
        const subpageArrow = assert(peoplePage.$$('#profile-subpage-arrow'));
        assertFalse(subpageArrow.hidden);
      });
    });

    suite('Chrome OS with account manager disabled', function() {
      suiteSetup(function() {
        loadTimeData.overrideValues({
          // Simulate SplitSettings (OS settings in their own surface).
          showOSSettings: false,
          // Disable ChromeOSAccountManager (Google Accounts support).
          isAccountManagerEnabled: false,
        });
      });

      setup(async function() {
        syncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

        profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ =
            profileInfoBrowserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        peoplePage.prefs = DEFAULT_PREFS;
        peoplePage.pageVisibility = settings.pageVisibility;
        document.body.appendChild(peoplePage);

        await syncBrowserProxy.whenCalled('getSyncStatus');
        Polymer.dom.flush();
      });

      teardown(function() {
        peoplePage.remove();
      });

      test('profile row is not actionable', () => {
        // Simulate a signed-in user.
        sync_test_util.simulateSyncStatus({
          signedIn: true,
        });

        // Account manager isn't available, so the row isn't actionable.
        const profileIcon = assert(peoplePage.$$('#profile-icon'));
        assertFalse(profileIcon.hasAttribute('actionable'));
        const profileRow = assert(peoplePage.$$('#profile-row'));
        assertFalse(profileRow.hasAttribute('actionable'));
        const subpageArrow = assert(peoplePage.$$('#profile-subpage-arrow'));
        assertTrue(subpageArrow.hidden);

        // Clicking on profile icon doesn't navigate to a new route.
        const oldRoute = settings.Router.getInstance().getCurrentRoute();
        profileIcon.click();
        assertEquals(oldRoute, settings.Router.getInstance().getCurrentRoute());
      });
    });

    suite('Chrome OS with SplitSettingsSync', function() {
      suiteSetup(function() {
        loadTimeData.overrideValues({
          splitSettingsSyncEnabled: true,
        });
      });

      setup(async function() {
        syncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

        profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ =
            profileInfoBrowserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        peoplePage.prefs = DEFAULT_PREFS;
        peoplePage.pageVisibility = settings.pageVisibility;
        document.body.appendChild(peoplePage);

        await syncBrowserProxy.whenCalled('getSyncStatus');
        Polymer.dom.flush();
      });

      teardown(function() {
        peoplePage.remove();
      });

      test('Sync account control is shown', () => {
        sync_test_util.simulateSyncStatus({
          signinAllowed: true,
          syncSystemEnabled: true,
        });

        // Account control is visible.
        const accountControl = peoplePage.$$('settings-sync-account-control');
        assertNotEquals(
            'none', window.getComputedStyle(accountControl).display);

        // Profile row items are not available.
        assertFalse(!!peoplePage.$$('#profile-icon'));
        assertFalse(!!peoplePage.$$('#profile-row'));
      });
    });
  }
});
