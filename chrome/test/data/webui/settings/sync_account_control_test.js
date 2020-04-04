// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {MAX_SIGNIN_PROMO_IMPRESSION, Router, routes, SyncBrowserProxyImpl, StatusAction} from 'chrome://settings/settings.js';
// #import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// #import {setupRouterWithSyncRoutes, simulateStoredAccounts} from 'chrome://test/settings/sync_test_util.m.js';
// #import {isChildVisible, isVisible} from 'chrome://test/test_util.m.js';
// clang-format on

cr.define('settings_sync_account_control', function() {

  suite('SyncAccountControl', function() {
    const peoplePage = null;
    let browserProxy = null;
    let testElement = null;

    /**
     * @param {number} count
     * @param {boolean} signedIn
     */
    function forcePromoResetWithCount(count, signedIn) {
      browserProxy.setImpressionCount(count);
      // Flipping syncStatus.signedIn will force promo state to be reset.
      testElement.syncStatus = {signedIn: !signedIn};
      testElement.syncStatus = {signedIn: signedIn};
    }

    setup(function() {
      sync_test_util.setupRouterWithSyncRoutes();
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();
      testElement = document.createElement('settings-sync-account-control');
      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'foo@foo.com'
      };
      testElement.prefs = {
        signin: {
          allowed_on_next_startup:
              {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
        },
      };
      document.body.appendChild(testElement);

      return browserProxy.whenCalled('getStoredAccounts').then(() => {
        Polymer.dom.flush();
        sync_test_util.simulateStoredAccounts([
          {
            fullName: 'fooName',
            givenName: 'foo',
            email: 'foo@foo.com',
          },
          {
            fullName: 'barName',
            givenName: 'bar',
            email: 'bar@bar.com',
          },
        ]);
      });
    });

    teardown(function() {
      testElement.remove();
    });

    test('promo shows/hides in the right states', function() {
      // Not signed in, no accounts, will show banner.
      sync_test_util.simulateStoredAccounts([]);
      forcePromoResetWithCount(0, false);
      const banner = testElement.$$('#banner');
      assertTrue(test_util.isVisible(banner));
      // Flipping signedIn in forcePromoResetWithCount should increment count.
      return browserProxy.whenCalled('incrementPromoImpressionCount')
          .then(() => {
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, false);
            assertFalse(test_util.isVisible(banner));

            // Not signed in, has accounts, will show banner.
            sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
            forcePromoResetWithCount(0, false);
            assertTrue(test_util.isVisible(banner));
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, false);
            assertFalse(test_util.isVisible(banner));

            // signed in, banners never show.
            sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
            forcePromoResetWithCount(0, true);
            assertFalse(test_util.isVisible(banner));
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, true);
            assertFalse(test_util.isVisible(banner));
          });
    });

    test('promo header has the correct class', function() {
      testElement.syncStatus = {signedIn: false, signedInUsername: ''};
      testElement.promoLabelWithNoAccount = testElement.promoLabelWithAccount =
          'title';
      sync_test_util.simulateStoredAccounts([]);
      assertTrue(test_util.isChildVisible(testElement, '#promo-header'));
      // When there is no secondary label, the settings box is one line.
      assertFalse(
          testElement.$$('#promo-header').classList.contains('two-line'));

      testElement.promoSecondaryLabelWithNoAccount =
          testElement.promoSecondaryLabelWithAccount = 'subtitle';
      // When there is a secondary label, the settings box is two line.
      assertTrue(
          testElement.$$('#promo-header').classList.contains('two-line'));
    });

    test('not signed in and no stored accounts', async function() {
      testElement.syncStatus = {signedIn: false, signedInUsername: ''};
      sync_test_util.simulateStoredAccounts([]);

      assertTrue(test_util.isChildVisible(testElement, '#promo-header'));
      assertFalse(test_util.isChildVisible(testElement, '#avatar-row'));
      // Chrome OS does not use the account switch menu.
      if (!cr.isChromeOS) {
        assertFalse(test_util.isChildVisible(testElement, '#menu'));
      }
      assertTrue(test_util.isChildVisible(testElement, '#sign-in'));

      testElement.$$('#sign-in').click();
      if (cr.isChromeOS) {
        await browserProxy.whenCalled('turnOnSync');
      } else {
        await browserProxy.whenCalled('startSignIn');
      }
    });

    test('not signed in but has stored accounts', function() {
      // Chrome OS users are always signed in.
      if (cr.isChromeOS) {
        return;
      }
      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: false,
        signedInUsername: '',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        disabled: false,
      };
      sync_test_util.simulateStoredAccounts([
        {
          fullName: 'fooName',
          givenName: 'foo',
          email: 'foo@foo.com',
        },
        {
          fullName: 'barName',
          givenName: 'bar',
          email: 'bar@bar.com',
        },
      ]);

      const userInfo = testElement.$$('#user-info');
      const syncButton = testElement.$$('#sync-button');

      // Avatar row shows the right account.
      assertTrue(test_util.isChildVisible(testElement, '#promo-header'));
      assertTrue(test_util.isChildVisible(testElement, '#avatar-row'));
      assertTrue(userInfo.textContent.includes('fooName'));
      assertTrue(userInfo.textContent.includes('foo@foo.com'));
      assertFalse(userInfo.textContent.includes('barName'));
      assertFalse(userInfo.textContent.includes('bar@bar.com'));

      // Menu contains the right items.
      assertTrue(!!testElement.$$('#menu'));
      assertFalse(testElement.$$('#menu').open);
      const items = testElement.root.querySelectorAll('.dropdown-item');
      assertEquals(4, items.length);
      assertTrue(items[0].textContent.includes('foo@foo.com'));
      assertTrue(items[1].textContent.includes('bar@bar.com'));
      assertEquals(items[2].id, 'sign-in-item');
      assertEquals(items[3].id, 'sign-out-item');

      // "sync to" button is showing the correct name and syncs with the
      // correct account when clicked.
      assertTrue(test_util.isVisible(syncButton));
      assertFalse(test_util.isChildVisible(testElement, '#turn-off'));
      syncButton.click();
      Polymer.dom.flush();

      return browserProxy.whenCalled('startSyncingWithEmail')
          .then((args) => {
            const email = args[0];
            const isDefaultPromoAccount = args[1];

            assertEquals(email, 'foo@foo.com');
            assertEquals(isDefaultPromoAccount, true);

            assertTrue(test_util.isChildVisible(testElement, 'cr-icon-button'));
            assertTrue(testElement.$$('#sync-icon-container').hidden);

            testElement.$$('#dropdown-arrow').click();
            Polymer.dom.flush();
            assertTrue(testElement.$$('#menu').open);

            // Switching selected account will update UI with the right name and
            // email.
            items[1].click();
            Polymer.dom.flush();
            assertFalse(userInfo.textContent.includes('fooName'));
            assertFalse(userInfo.textContent.includes('foo@foo.com'));
            assertTrue(userInfo.textContent.includes('barName'));
            assertTrue(userInfo.textContent.includes('bar@bar.com'));
            assertTrue(test_util.isVisible(syncButton));

            browserProxy.resetResolver('startSyncingWithEmail');
            syncButton.click();
            Polymer.dom.flush();

            return browserProxy.whenCalled('startSyncingWithEmail');
          })
          .then((args) => {
            const email = args[0];
            const isDefaultPromoAccount = args[1];
            assertEquals(email, 'bar@bar.com');
            assertEquals(isDefaultPromoAccount, false);

            // Tapping the last menu item will initiate sign-in.
            items[2].click();
            return browserProxy.whenCalled('startSignIn');
          });
    });

    test('signed in, no error', function() {
      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: false,
      };
      Polymer.dom.flush();

      assertTrue(test_util.isChildVisible(testElement, '#avatar-row'));
      assertFalse(test_util.isChildVisible(testElement, '#promo-header'));
      assertFalse(testElement.$$('#sync-icon-container').hidden);

      // Chrome OS does not use the account switch menu.
      if (!cr.isChromeOS) {
        assertFalse(test_util.isChildVisible(testElement, 'cr-icon-button'));
        assertFalse(!!testElement.$$('#menu'));
      }

      const userInfo = testElement.$$('#user-info');
      assertTrue(userInfo.textContent.includes('barName'));
      assertTrue(userInfo.textContent.includes('bar@bar.com'));
      assertFalse(userInfo.textContent.includes('fooName'));
      assertFalse(userInfo.textContent.includes('foo@foo.com'));

      assertFalse(test_util.isChildVisible(testElement, '#sync-button'));
      assertTrue(test_util.isChildVisible(testElement, '#turn-off'));
      assertFalse(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.$$('#avatar-row #turn-off').click();
      Polymer.dom.flush();

      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.Router.getInstance().getRoutes().SIGN_OUT);
    });

    test('signed in, has error', function() {
      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.CONFIRM_SYNC_SETTINGS,
        disabled: false,
      };
      Polymer.dom.flush();
      const userInfo = testElement.$$('#user-info');

      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-problem'));
      assertTrue(!!testElement.$$('[icon="settings:sync-problem"]'));
      let displayedText =
          userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertTrue(displayedText.includes('Sync isn\'t working'));
      // The sync error button is shown to resolve the error.
      assertTrue(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.REAUTHENTICATE,
        disabled: false,
      };
      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-paused'));
      assertTrue(!!testElement.$$('[icon=\'settings:sync-disabled\']'));
      displayedText = userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertTrue(displayedText.includes('Sync is paused'));
      // The sync error button is shown to resolve the error.
      assertTrue(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: true,
      };

      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-disabled'));
      assertTrue(!!testElement.$$('[icon=\'cr:sync\']'));
      displayedText = userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertTrue(displayedText.includes('Sync disabled'));
      assertFalse(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.REAUTHENTICATE,
        hasError: true,
        hasUnrecoverableError: true,
        disabled: false,
      };
      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-problem'));
      assertTrue(!!testElement.$$('[icon="settings:sync-problem"]'));
      displayedText = userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertTrue(displayedText.includes('Sync isn\'t working'));

      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS,
        hasError: true,
        hasPasswordsOnlyError: true,
        hasUnrecoverableError: false,
        disabled: false,
      };
      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-problem'));
      assertTrue(!!testElement.$$('[icon="settings:sync-problem"]'));
      displayedText = userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertFalse(displayedText.includes('Sync isn\'t working'));
      assertTrue(displayedText.includes('Error syncing passwords'));
      // The sync error button is shown to resolve the error.
      assertTrue(test_util.isChildVisible(testElement, '#sync-error-button'));
      assertTrue(test_util.isChildVisible(testElement, '#turn-off'));
    });

    test('signed in, setup in progress', function() {
      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        statusText: 'Setup in progress...',
        firstSetupInProgress: true,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: false,
      };
      Polymer.dom.flush();
      const userInfo = testElement.$$('#user-info');
      const setupButtons = testElement.$$('#setup-buttons');

      assertTrue(userInfo.textContent.includes('barName'));
      assertTrue(userInfo.textContent.includes('Setup in progress...'));
      assertTrue(test_util.isVisible(setupButtons));
    });

    test('embedded in another page', function() {
      testElement.embeddedInSubpage = true;
      forcePromoResetWithCount(100, false);
      const banner = testElement.$$('#banner');
      assertTrue(test_util.isVisible(banner));

      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: false,
      };

      assertTrue(test_util.isChildVisible(testElement, '#turn-off'));
      assertFalse(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.embeddedInSubpage = true;
      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.REAUTHENTICATE,
        disabled: false,
      };
      assertTrue(test_util.isChildVisible(testElement, '#turn-off'));
      assertTrue(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.embeddedInSubpage = true;
      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: true,
        statusAction: settings.StatusAction.REAUTHENTICATE,
        disabled: false,
      };
      assertTrue(test_util.isChildVisible(testElement, '#turn-off'));
      assertTrue(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.embeddedInSubpage = true;
      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.ENTER_PASSPHRASE,
        disabled: false,
      };
      assertTrue(test_util.isChildVisible(testElement, '#turn-off'));
      // Don't show passphrase error button on embedded page.
      assertFalse(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.embeddedInSubpage = true;
      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: true,
        statusAction: settings.StatusAction.NO_ACTION,
        disabled: false,
      };
      assertTrue(test_util.isChildVisible(testElement, '#turn-off'));
      assertFalse(test_util.isChildVisible(testElement, '#sync-error-button'));
    });

    test('hide buttons', function() {
      testElement.hideButtons = true;
      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: false,
      };

      assertFalse(test_util.isChildVisible(testElement, '#turn-off'));
      assertFalse(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.REAUTHENTICATE,
        disabled: false,
      };
      assertFalse(test_util.isChildVisible(testElement, '#turn-off'));
      assertFalse(test_util.isChildVisible(testElement, '#sync-error-button'));

      testElement.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.ENTER_PASSPHRASE,
        disabled: false,
      };
      assertFalse(test_util.isChildVisible(testElement, '#turn-off'));
      assertFalse(test_util.isChildVisible(testElement, '#sync-error-button'));
    });

    test('signinButtonDisabled', function() {
      // Ensure that the sync button is disabled when signin is disabled.
      assertFalse(testElement.$$('#sign-in').disabled);
      testElement.setPrefValue('signin.allowed_on_next_startup', false);
      Polymer.dom.flush();
      assertTrue(testElement.$$('#sign-in').disabled);
    });
  });
  // #cr_define_end
});
