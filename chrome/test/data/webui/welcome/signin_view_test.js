// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('signin_view_test', function() {
  suite('SigninViewTest', function() {

    /** @type {SigninViewElement} */
    let testElement;

    /** @type {welcome.WelcomeBrowserProxy} */
    let testWelcomeBrowserProxy;

    setup(function() {
      testWelcomeBrowserProxy = new TestWelcomeBrowserProxy();
      welcome.WelcomeBrowserProxyImpl.instance_ = testWelcomeBrowserProxy;

      PolymerTest.clearBody();
      testElement = document.createElement('signin-view');
      document.body.appendChild(testElement);
    });

    teardown(function() {
      testElement.remove();
    });

    test('sign-in button', function() {
      const signinButton = testElement.$$('cr-button');
      assertTrue(!!signinButton);

      signinButton.click();
      return testWelcomeBrowserProxy.whenCalled('handleActivateSignIn')
          .then(redirectUrl => assertEquals(null, redirectUrl));
    });

    test('no-thanks button', function() {
      const noThanksButton = testElement.$$('button');
      assertTrue(!!noThanksButton);
      noThanksButton.click();
      return testWelcomeBrowserProxy.whenCalled('handleUserDecline');
    });
  });
});
