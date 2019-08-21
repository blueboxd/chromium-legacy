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
          pic: 'data:image/png;base64,primaryAccountPicData',
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
          pic: '',
        },
        {
          id: '789',
          accountType: 1,
          isDeviceAccount: false,
          isSignedIn: false,
          unmigrated: false,
          fullName: 'Secondary Account 2',
          email: 'user2@example.com',
          pic: '',
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

  suite('PeoplePageTests', function() {
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
    });

    teardown(function() {
      peoplePage.remove();
    });

    test('Profile name and picture, account manager disabled', async () => {
      loadTimeData.overrideValues({
        isAccountManagerEnabled: false,
      });
      peoplePage = document.createElement('os-settings-people-page');
      peoplePage.pageVisibility = settings.pageVisibility;
      document.body.appendChild(peoplePage);

      await browserProxy.whenCalled('getProfileInfo');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      Polymer.dom.flush();

      assertEquals(
          browserProxy.fakeProfileInfo.name,
          peoplePage.$$('#profile-name').textContent.trim());
      const bg = peoplePage.$$('#profile-icon').style.backgroundImage;
      assertTrue(bg.includes(browserProxy.fakeProfileInfo.iconUrl));

      const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
          'LAAAAAABAAEAAAICTAEAOw==';
      cr.webUIListenerCallback(
          'profile-info-changed', {name: 'pushedName', iconUrl: iconDataUrl});

      Polymer.dom.flush();
      assertEquals(
          'pushedName', peoplePage.$$('#profile-name').textContent.trim());
      const newBg = peoplePage.$$('#profile-icon').style.backgroundImage;
      assertTrue(newBg.includes(iconDataUrl));

      // Sub-page trigger is hidden.
      assertTrue(peoplePage.$$('#account-manager-subpage-trigger').hidden);
    });

    test('GAIA name and picture, account manager enabled', async () => {
      loadTimeData.overrideValues({
        isAccountManagerEnabled: true,
      });
      peoplePage = document.createElement('os-settings-people-page');
      peoplePage.pageVisibility = settings.pageVisibility;
      document.body.appendChild(peoplePage);

      await accountManagerBrowserProxy.whenCalled('getAccounts');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      Polymer.dom.flush();

      chai.assert.include(
          peoplePage.$$('#profile-icon').style.backgroundImage,
          'data:image/png;base64,primaryAccountPicData');
      assertEquals(
          'Primary Account', peoplePage.$$('#profile-name').textContent.trim());

      // Rather than trying to mock cr.sendWithPromise('getPluralString', ...)
      // just force an update.
      await peoplePage.updateAccounts_();
      assertEquals(
          'primary@gmail.com, +2 more accounts',
          peoplePage.$$('#profile-label').textContent.trim());

      // Sub-page trigger is shown.
      const subpageTrigger = peoplePage.$$('#account-manager-subpage-trigger');
      assertFalse(subpageTrigger.hidden);

      // Sub-page trigger navigates to Google account manager.
      subpageTrigger.click();
      assertEquals(settings.getCurrentRoute(), settings.routes.ACCOUNT_MANAGER);
    });
  });
});
