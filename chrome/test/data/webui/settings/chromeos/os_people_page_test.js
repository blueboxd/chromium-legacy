// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page', function() {
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

      return Promise.resolve([
        {
          id: '123',
          accountType: 1,
          isDeviceAccount: true,
          isSignedIn: true,
          unmigrated: false,
          fullName: 'Primary Account',
          email: 'primary@gmail.com',
        },
        {
          id: '456',
          accountType: 1,
          isDeviceAccount: false,
          isSignedIn: true,
          unmigrated: false,
          fullName: 'Secondary Account 1',
          email: 'user1@example.com',
        },
        {
          id: '789',
          accountType: 1,
          isDeviceAccount: false,
          isSignedIn: false,
          unmigrated: false,
          fullName: 'Secondary Account 2',
          email: 'user2@example.com',
        },
      ]);
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

  suite('ProfileInfoTests', function() {
    /** @type {SettingsPeoplePageElement} */
    let peoplePage = null;
    /** @type {settings.ProfileInfoBrowserProxy} */
    let browserProxy = null;
    /** @type {settings.SyncBrowserProxy} */
    let syncBrowserProxy = null;
    /** @type {settings.AccountManagerBrowserProxy} */
    let accountManagerBrowserProxy = null;

    setup(function() {
      browserProxy = new TestProfileInfoBrowserProxy();
      settings.ProfileInfoBrowserProxyImpl.instance_ = browserProxy;

      syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

      accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
      settings.AccountManagerBrowserProxyImpl.instance_ =
          accountManagerBrowserProxy;

      PolymerTest.clearBody();
      peoplePage = document.createElement('os-settings-people-page');
      peoplePage.pageVisibility = settings.pageVisibility;
      document.body.appendChild(peoplePage);

      return Promise
          .all([
            browserProxy.whenCalled('getProfileInfo'),
            syncBrowserProxy.whenCalled('getSyncStatus'),
            accountManagerBrowserProxy.whenCalled('getAccounts')
          ])
          .then(function() {
            Polymer.dom.flush();
          });
    });

    teardown(function() {
      peoplePage.remove();
    });

    test('GetProfileInfo', async function() {
      assertEquals(
          browserProxy.fakeProfileInfo.name,
          peoplePage.$$('#profile-name').textContent.trim());

      cr.webUIListenerCallback(
          'profile-info-changed', {name: 'pushedName', iconUrl: ''});

      Polymer.dom.flush();
      assertEquals(
          'pushedName', peoplePage.$$('#profile-name').textContent.trim());

      // Rather than trying to mock cr.sendWithPromise('getPluralString', ...)
      // just force an update.
      await peoplePage.updateProfileLabel_();
      assertEquals(
          'primary@gmail.com, +2 more accounts',
          peoplePage.$$('#profile-label').textContent.trim());
    });
  });
});
