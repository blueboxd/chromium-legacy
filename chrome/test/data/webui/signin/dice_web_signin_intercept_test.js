// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_app.js';

import {DiceWebSigninInterceptBrowserProxyImpl} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';

import {assertTrue} from '../chai_assert.js';
import {isChildVisible} from '../test_util.m.js';

import {TestDiceWebSigninInterceptBrowserProxy} from './test_dice_web_signin_intercept_browser_proxy.js';

suite('DiceWebSigninInterceptTest', function() {
  /** @type {!DiceWebSigninInterceptAppElement} */
  let app;

  /** @type {!TestDiceWebSigninInterceptBrowserProxy} */
  let browserProxy;

  setup(function() {
    browserProxy = new TestDiceWebSigninInterceptBrowserProxy();
    DiceWebSigninInterceptBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    app = /** @type {!DiceWebSigninInterceptAppElement} */ (
        document.createElement('dice-web-signin-intercept-app'));
    document.body.append(app);
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
});
