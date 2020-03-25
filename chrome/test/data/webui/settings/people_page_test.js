// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://settings/lazy_load.js';
// #import {ProfileInfoBrowserProxyImpl, Router, StatusAction, SyncBrowserProxyImpl, pageVisibility, routes} from 'chrome://settings/settings.js';
// #import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';
// #import {TestProfileInfoBrowserProxy} from 'chrome://test/settings/test_profile_info_browser_proxy.m.js';
// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {isChromeOS, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
// #import {listenOnce} from 'chrome://resources/js/util.m.js';
// #import {simulateSyncStatus, simulateStoredAccounts} from 'chrome://test/settings/sync_test_util.m.js';
// #import {waitBeforeNextRender} from 'chrome://test/test_util.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// clang-format on

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
    suite('SigninDisallowedTests', function() {
      setup(function() {
        loadTimeData.overrideValues({signinAllowed: false});

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

      test('ShowCorrectRows', function() {
        return syncBrowserProxy.whenCalled('getSyncStatus').then(function() {
          Polymer.dom.flush();

          // The correct /manageProfile link row is shown.
          assertFalse(!!peoplePage.$$('#edit-profile'));
          assertTrue(!!peoplePage.$$('#picture-subpage-trigger'));

          // Control element doesn't exist when policy forbids sync.
          sync_test_util.simulateSyncStatus({
            signedIn: false,
            syncSystemEnabled: true,
          });
          assertFalse(!!peoplePage.$$('settings-sync-account-control'));
        });
      });
    });

    suite('SyncStatusTests', function() {
      setup(async function() {
        loadTimeData.overrideValues({signinAllowed: true});
        syncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

        profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ =
            profileInfoBrowserProxy;

        PolymerTest.clearBody();
        /* #ignore */ await settings.forceLazyLoaded();
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

      test('ShowCorrectRows', function() {
        return syncBrowserProxy.whenCalled('getSyncStatus').then(function() {
          sync_test_util.simulateSyncStatus({
            syncSystemEnabled: true,
          });
          Polymer.dom.flush();

          // The correct /manageProfile link row is shown.
          assertTrue(!!peoplePage.$$('#edit-profile'));
          assertFalse(!!peoplePage.$$('#picture-subpage-trigger'));

          sync_test_util.simulateSyncStatus({
            signedIn: false,
            syncSystemEnabled: true,
          });

          // The control element should exist when policy allows.
          const accountControl = peoplePage.$$('settings-sync-account-control');
          assertTrue(
              window.getComputedStyle(accountControl)['display'] != 'none');

          // Control element doesn't exist when policy forbids sync.
          sync_test_util.simulateSyncStatus({
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

      test('SignOutNavigationNormalProfile', function() {
        // Navigate to chrome://settings/signOut
        settings.Router.getInstance().navigateTo(settings.routes.SIGN_OUT);

        return new Promise(function(resolve) {
                 peoplePage.async(resolve);
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
            });
      });

      test('SignOutDialogManagedProfile', function() {
        let accountControl = null;
        return syncBrowserProxy.whenCalled('getSyncStatus')
            .then(function() {
              sync_test_util.simulateSyncStatus({
                signedIn: true,
                domain: 'example.com',
                syncSystemEnabled: true,
              });

              assertFalse(!!peoplePage.$$('#dialog'));
              accountControl = peoplePage.$$('settings-sync-account-control');
              return test_util.waitBeforeNextRender(accountControl);
            })
            .then(function() {
              const turnOffButton = accountControl.$$('#turn-off');
              turnOffButton.click();
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
        // Navigate to chrome://settings/signOut
        settings.Router.getInstance().navigateTo(settings.routes.SIGN_OUT);

        return new Promise(function(resolve) {
                 peoplePage.async(resolve);
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

  // #cr_define_end
});
