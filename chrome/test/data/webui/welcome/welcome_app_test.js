// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_welcome_app_test', function() {
  suite('WelcomeAppTest', function() {

    /** @type {WelcomeAppElement} */
    let testElement;

    /** @type {welcome.WelcomeBrowserProxy} */
    let testWelcomeBrowserProxy;

    /** @type {welcome.NuxSetAsDefaultProxy} */
    let testSetAsDefaultProxy;

    function resetTestElement() {
      PolymerTest.clearBody();
      welcome.navigateTo(welcome.Routes.LANDING, 'landing');
      testElement = document.createElement('welcome-app');
      document.body.appendChild(testElement);
    }

    function simulateCanSetDefault() {
      testSetAsDefaultProxy.setDefaultStatus({
        isDefault: false,
        canBeDefault: true,
        isDisabledByPolicy: false,
        isUnknownError: false,
      });

      resetTestElement();
    }

    function simulateCannotSetDefault() {
      testSetAsDefaultProxy.setDefaultStatus({
        isDefault: true,
        canBeDefault: true,
        isDisabledByPolicy: false,
        isUnknownError: false,
      });

      resetTestElement();
    }

    setup(function() {
      testWelcomeBrowserProxy = new TestWelcomeBrowserProxy();
      welcome.WelcomeBrowserProxyImpl.instance_ = testWelcomeBrowserProxy;

      testSetAsDefaultProxy = new TestNuxSetAsDefaultProxy();
      welcome.NuxSetAsDefaultProxyImpl.instance_ = testSetAsDefaultProxy;

      // Not used in test, but setting to test proxy anyway, in order to prevent
      // calls to backend.
      welcome.BookmarkProxyImpl.instance_ = new TestBookmarkProxy();

      resetTestElement();
    });

    teardown(function() {
      testElement.remove();
    });

    test('shows landing page by default', function() {
      assertEquals(
          testElement.shadowRoot.querySelectorAll('[slot=view]').length, 1);
      assertTrue(!!testElement.$$('landing-view'));
      assertTrue(testElement.$$('landing-view').classList.contains('active'));
    });

    test('new user route (can set default)', function() {
      simulateCanSetDefault();
      welcome.navigateTo(welcome.Routes.NEW_USER, 1);
      return test_util.waitForRender(testElement).then(() => {
        const views = testElement.shadowRoot.querySelectorAll('[slot=view]');
        assertEquals(views.length, 5);
        ['LANDING-VIEW',
         'NUX-GOOGLE-APPS',
         'NUX-NTP-BACKGROUND',
         'NUX-SET-AS-DEFAULT',
         'SIGNIN-VIEW',
        ].forEach((expectedView, ix) => {
          assertEquals(expectedView, views[ix].tagName);
        });
      });
    });

    test('new user route (cannot set default)', function() {
      simulateCannotSetDefault();
      welcome.navigateTo(welcome.Routes.NEW_USER, 1);
      return test_util.waitForRender(testElement).then(() => {
        const views = testElement.shadowRoot.querySelectorAll('[slot=view]');
        assertEquals(views.length, 4);
        ['LANDING-VIEW',
         'NUX-GOOGLE-APPS',
         'NUX-NTP-BACKGROUND',
         'SIGNIN-VIEW',
        ].forEach((expectedView, ix) => {
          assertEquals(expectedView, views[ix].tagName);
        });
      });
    });

    test('returning user route (can set default)', function() {
      simulateCanSetDefault();
      welcome.navigateTo(welcome.Routes.RETURNING_USER, 1);
      return test_util.waitForRender(testElement).then(() => {
        const views = testElement.shadowRoot.querySelectorAll('[slot=view]');
        assertEquals(views.length, 2);
        assertEquals(views[0].tagName, 'LANDING-VIEW');
        assertEquals(views[1].tagName, 'NUX-SET-AS-DEFAULT');
      });
    });

    test('returning user route (cannot set default)', function() {
      simulateCannotSetDefault();
      welcome.navigateTo(welcome.Routes.RETURNING_USER, 1);

      // At this point, there should be no steps in the returning user, so
      // welcome_app should try to go to NTP.
      return testWelcomeBrowserProxy.whenCalled('goToNewTabPage');
    });

    test('default-status check resolves with correct value', function() {
      /**
       * @param {!welcome.DefaultBrowserInfo} status
       * @param {boolean} expectedDefaultExists
       * @return {!Promise}
       */
      function checkDefaultStatusResolveValue(status, expectedDefaultExists) {
        testSetAsDefaultProxy.setDefaultStatus(status);
        resetTestElement();

        // Use the new-user route to test if nux-set-as-default module gets
        // initialized.
        welcome.navigateTo(welcome.Routes.NEW_USER, 1);
        return test_util.waitForRender(testElement).then(() => {
          // Use the existence of the nux-set-as-default as indication of
          // whether or not the promise is resolved with the expected result.
          assertEquals(
              expectedDefaultExists, !!testElement.$$('nux-set-as-default'));
        });
      }

      return checkDefaultStatusResolveValue(
                 {
                   // Allowed to set as default, and not default yet.
                   isDefault: false,
                   canBeDefault: true,
                   isDisabledByPolicy: false,
                   isUnknownError: false,
                 },
                 true)
          .then(() => {
            return checkDefaultStatusResolveValue(
                {
                  // Allowed to set as default, but already default.
                  isDefault: true,
                  canBeDefault: true,
                  isDisabledByPolicy: false,
                  isUnknownError: false,
                },
                false);
          })
          .then(() => {
            return checkDefaultStatusResolveValue(
                {
                  // Not default yet, but this chrome install cannot be default.
                  isDefault: false,
                  canBeDefault: false,
                  isDisabledByPolicy: false,
                  isUnknownError: false,
                },
                false);
          })
          .then(() => {
            return checkDefaultStatusResolveValue(
                {
                  // Not default yet, but setting default is disabled by policy.
                  isDefault: false,
                  canBeDefault: true,
                  isDisabledByPolicy: true,
                  isUnknownError: false,
                },
                false);
          })
          .then(() => {
            return checkDefaultStatusResolveValue(
                {
                  // Not default yet, but there is some unknown error.
                  isDefault: false,
                  canBeDefault: true,
                  isDisabledByPolicy: false,
                  isUnknownError: true,
                },
                false);
          });
    });
  });
});
