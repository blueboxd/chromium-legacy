// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signin-reauth/signin_reauth_app.js';

import {SigninReauthBrowserProxyImpl} from 'chrome://signin-reauth/signin_reauth_browser_proxy.js';
import {TestSigninReauthBrowserProxy} from './test_signin_reauth_browser_proxy.js';

suite('SigninReauthTest', function() {
  let app;

  /** @type {TestSigninReauthBrowserProxy} */
  let browserProxy;

  setup(function() {
    browserProxy = new TestSigninReauthBrowserProxy();
    SigninReauthBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    app = document.createElement('signin-reauth-app');
    document.body.append(app);
  });

  // Tests that no DCHECKS are thrown during initialization of the UI.
  test('LoadPage', function() {
    // This test makes comparisons with strings in their default locale,
    // which is en-US.
    assertEquals(
        'en-US', navigator.language,
        'Cannot verify strings for the ' + navigator.language + 'locale.');

    assertEquals(
        'Store once, use everywhere',
        app.$.signinReauthTitle.textContent.trim());
  });

  test('ClickConfirm', function() {
    app.$.confirmButton.click();
    return browserProxy.whenCalled('confirm');
  });

  test('ClickCancel', function() {
    app.$.cancelButton.click();
    return browserProxy.whenCalled('cancel');
  });
});
