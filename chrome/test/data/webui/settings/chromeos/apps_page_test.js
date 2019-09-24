// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?OsSettingsAppsPageElement} */
let appsPage = null;

/** @type {?TestAndroidAppsBrowserProxy} */
let androidAppsBrowserProxy = null;

const setAndroidAppsState = function(playStoreEnabled, settingsAppAvailable) {
  const appsInfo = {
    playStoreEnabled: playStoreEnabled,
    settingsAppAvailable: settingsAppAvailable,
  };
  appsPage.androidAppsInfo = appsInfo;
  appsPage.showAndroidApps = true;
  Polymer.dom.flush();
};

suite('AppsPageTests', function() {
  setup(function() {
    PolymerTest.clearBody();
    appsPage = document.createElement('os-settings-apps-page');
    document.body.appendChild(appsPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    appsPage.remove();
  });

  suite('Page Combinations', function() {
    setup(function() {
      appsPage.havePlayStoreApp = true;
      appsPage.prefs = {arc: {enabled: {value: false}}};
    });

    const AndroidAppsShown = () => !!appsPage.$$('#android-apps');
    const AppManagementShown = () => !!appsPage.$$('#appManagement');

    test('Nothing Shown', function() {
      appsPage.showAppManagement = false;
      appsPage.showAndroidApps = false;
      Polymer.dom.flush();

      assertFalse(AppManagementShown());
      assertFalse(AndroidAppsShown());
    });

    test('Only Android Apps Shown', function() {
      appsPage.showAppManagement = false;
      appsPage.showAndroidApps = true;
      Polymer.dom.flush();

      assertFalse(AppManagementShown());
      assertTrue(AndroidAppsShown());
    });

    test('Only App Management Shown', function() {
      appsPage.showAppManagement = true;
      appsPage.showAndroidApps = false;
      Polymer.dom.flush();

      assertTrue(AppManagementShown());
      assertFalse(AndroidAppsShown());
    });

    test('Android Apps and App Management Shown', function() {
      appsPage.showAppManagement = true;
      appsPage.showAndroidApps = true;
      Polymer.dom.flush();

      assertTrue(AppManagementShown());
      assertTrue(AndroidAppsShown());
    });
  });
});

// Changes to this suite should be reflected in android_apps_page_test.js
suite('AndroidAppsDetailPageTests', function() {
  setup(function() {
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    settings.AndroidAppsBrowserProxyImpl.instance_ = androidAppsBrowserProxy;
    PolymerTest.clearBody();
    appsPage = document.createElement('os-settings-apps-page');
    document.body.appendChild(appsPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    appsPage.remove();
  });

  suite('Main Page', function() {
    setup(function() {
      appsPage.havePlayStoreApp = true;
      appsPage.prefs = {arc: {enabled: {value: false}}};
      setAndroidAppsState(false, false);
    });

    test('Enable', function() {
      const button = appsPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!appsPage.$$('.subpage-arrow'));

      button.click();
      Polymer.dom.flush();
      assertTrue(appsPage.prefs.arc.enabled.value);

      setAndroidAppsState(true, false);
      assertTrue(!!appsPage.$$('.subpage-arrow'));
    });
  });

  // TODO(crbug.com/1006662): Fix test suite.
  suite.skip('SubPage', function() {
    let subpage;

    function flushAsync() {
      Polymer.dom.flush();
      return new Promise(resolve => {
        appsPage.async(resolve);
      });
    }

    /**
     * Returns a new promise that resolves after a window 'popstate' event.
     * @return {!Promise}
     */
    function whenPopState() {
      return new Promise(function(resolve) {
        window.addEventListener('popstate', function callback() {
          window.removeEventListener('popstate', callback);
          resolve();
        });
      });
    }

    setup(function() {
      appsPage.havePlayStoreApp = true;
      appsPage.prefs = {arc: {enabled: {value: true}}};
      setAndroidAppsState(true, false);
      settings.navigateTo(settings.routes.ANDROID_APPS);
      appsPage.$$('#android-apps').click();
      return flushAsync().then(() => {
        subpage = appsPage.$$('settings-android-apps-subpage');
        assertTrue(!!subpage);
      });
    });

    test('Sanity', function() {
      assertTrue(!!subpage.$$('#remove'));
      assertTrue(!subpage.$$('#manageApps'));
    });

    test('ManageAppsUpdate', function() {
      assertTrue(!subpage.$$('#manageApps'));
      setAndroidAppsState(true, true);
      assertTrue(!!subpage.$$('#manageApps'));
      setAndroidAppsState(true, false);
      assertTrue(!subpage.$$('#manageApps'));
    });

    test('ManageAppsOpenRequest', function() {
      setAndroidAppsState(true, true);
      const button = subpage.$$('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      Polymer.dom.flush();
      return promise;
    });

    test('Disable', function() {
      const dialog = subpage.$$('#confirmDisableDialog');
      assertTrue(!!dialog);
      assertFalse(dialog.open);

      const remove = subpage.$$('#remove');
      assertTrue(!!remove);

      subpage.onRemoveTap_();
      Polymer.dom.flush();
      assertTrue(dialog.open);
      dialog.close();
    });

    test('HideOnDisable', function() {
      assertEquals(
          settings.getCurrentRoute(), settings.routes.ANDROID_APPS_DETAILS);
      setAndroidAppsState(false, false);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.ANDROID_APPS);
      });
    });
  });

  // TODO(crbug.com/1006662): Fix test suite.
  suite.skip('Enforced', function() {
    let subpage;

    setup(function() {
      appsPage.havePlayStoreApp = true;
      appsPage.prefs = {
        arc: {
          enabled: {
            value: true,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED
          }
        }
      };
      setAndroidAppsState(true, true);
      assertTrue(!!settings.routes.ANDROID_APPS_DETAILS);
      appsPage.$$('#android-apps').click();
      Polymer.dom.flush();
      subpage = appsPage.$$('settings-android-apps-subpage');
      assertTrue(!!subpage);
    });

    test('Sanity', function(done) {
      Polymer.dom.flush();
      assertFalse(!!subpage.$$('#remove'));
      assertTrue(!!subpage.$$('#manageApps'));
    });
  });

  suite('NoPlayStore', function() {
    setup(function() {
      appsPage.havePlayStoreApp = false;
      appsPage.prefs = {arc: {enabled: {value: true}}};
      setAndroidAppsState(true, true);
    });

    test('Sanity', function() {
      assertTrue(!!appsPage.$$('#manageApps'));
    });

    test('ManageAppsOpenRequest', function() {
      const button = appsPage.$$('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      Polymer.dom.flush();
      return promise;
    });
  });
});
