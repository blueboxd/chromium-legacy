// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {AccountInfo, DiceWebSigninInterceptBrowserProxyImpl} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {isChildVisible} from '../test_util.m.js';

import {TestDiceWebSigninInterceptBrowserProxy} from './test_dice_web_signin_intercept_browser_proxy.js';

/** @param {!AccountInfo} account_info */
function fireAccountInfoChanged(account_info) {
  webUIListenerCallback('account-info-changed', account_info);
}

suite('DiceWebSigninInterceptTest', function() {
  /** @type {!DiceWebSigninInterceptAppElement} */
  let app;

  /** @type {!TestDiceWebSigninInterceptBrowserProxy} */
  let browserProxy;

  /** @type {string} */
  const PLACEHOLDER_AVATAR =
      'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';

  setup(function() {
    browserProxy = new TestDiceWebSigninInterceptBrowserProxy();
    browserProxy.setAccountInfo({pictureUrl: PLACEHOLDER_AVATAR, name: ''});
    DiceWebSigninInterceptBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    app = /** @type {!DiceWebSigninInterceptAppElement} */ (
        document.createElement('dice-web-signin-intercept-app'));
    document.body.append(app);
    return browserProxy.whenCalled('pageLoaded');
  });

  test('ClickAccept', function() {
    assertTrue(isChildVisible(app, '#acceptButton'));
    app.$$('#acceptButton').click();
    return browserProxy.whenCalled('accept');
  });

  test('ClickCancel', function() {
    assertTrue(isChildVisible(app, '#cancelButton'));
    app.$$('#cancelButton').click();
    return browserProxy.whenCalled('cancel');
  });

  test('AccountInfo', function() {
    assertTrue(isChildVisible(app, '#userAvatar'));
    assertFalse(isChildVisible(app, '#accountName'));
    const userImg = app.$$('#userAvatar');
    const accountNameElement = app.$$('#accountName');
    assertEquals(PLACEHOLDER_AVATAR, userImg.src);
    const avatarUrl = 'chrome://theme/IDR_PROFILE_AVATAR_0';
    const accountName = 'Foo';
    fireAccountInfoChanged({pictureUrl: avatarUrl, name: accountName});
    assertEquals(avatarUrl, userImg.src);
    assertEquals(accountName, accountNameElement.textContent);
    assertTrue(isChildVisible(app, '#accountName'));
  });
});
