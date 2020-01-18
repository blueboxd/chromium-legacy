// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('sync_test_util', function() {
  /**
   * Returns sync prefs with everything synced and no passphrase required.
   * @return {!settings.SyncPrefs}
   */
  function getSyncAllPrefs() {
    return {
      appsRegistered: true,
      appsSynced: true,
      autofillRegistered: true,
      autofillSynced: true,
      bookmarksRegistered: true,
      bookmarksSynced: true,
      encryptAllData: false,
      encryptAllDataAllowed: true,
      enterPassphraseBody: 'Enter custom passphrase.',
      extensionsRegistered: true,
      extensionsSynced: true,
      fullEncryptionBody: '',
      passphrase: '',
      passphraseRequired: false,
      passwordsRegistered: true,
      passwordsSynced: true,
      paymentsIntegrationEnabled: true,
      preferencesRegistered: true,
      preferencesSynced: true,
      setNewPassphrase: false,
      syncAllDataTypes: true,
      tabsRegistered: true,
      tabsSynced: true,
      themesRegistered: true,
      themesSynced: true,
      typedUrlsRegistered: true,
      typedUrlsSynced: true,
    };
  }

  function setupRouterWithSyncRoutes() {
    const routes = {
      BASIC: new settings.Route('/'),
    };
    routes.PEOPLE = routes.BASIC.createSection('/people', 'people');
    routes.SYNC = routes.PEOPLE.createChild('/syncSetup');
    routes.SYNC_ADVANCED = routes.SYNC.createChild('/syncSetup/advanced');

    routes.SIGN_OUT = routes.BASIC.createChild('/signOut');
    routes.SIGN_OUT.isNavigableDialog = true;

    settings.Router.resetInstanceForTesting(new settings.Router(routes));
    settings.routes = routes;
  }

  /** @param {!settings.SyncStatus} */
  function simulateSyncStatus(status) {
    cr.webUIListenerCallback('sync-status-changed', status);
    Polymer.dom.flush();
  }

  /** @param {Array<!settings.StoredAccount>} */
  function simulateStoredAccounts(accounts) {
    cr.webUIListenerCallback('stored-accounts-updated', accounts);
    Polymer.dom.flush();
  }

  return {
    getSyncAllPrefs: getSyncAllPrefs,
    setupRouterWithSyncRoutes: setupRouterWithSyncRoutes,
    simulateSyncStatus: simulateSyncStatus,
    simulateStoredAccounts: simulateStoredAccounts,
  };
});
