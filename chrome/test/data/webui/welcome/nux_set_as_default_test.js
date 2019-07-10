// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_set_as_default_test', function() {
  suite('SetAsDefaultTest', function() {
    /** @type {NuxSetAsDefaultElement} */
    let testElement;

    /** @type {welcome.NuxSetAsDefaultProxy} */
    let testSetAsDefaultProxy;

    /** @type {!Promise} */
    let navigatedPromise;

    setup(function() {
      testSetAsDefaultProxy = new TestNuxSetAsDefaultProxy();
      welcome.NuxSetAsDefaultProxyImpl.instance_ = testSetAsDefaultProxy;

      navigatedPromise = new Promise(resolve => {
        // Spy on navigational function to make sure it's called.
        welcome.navigateToNextStep = () => resolve();
      });

      PolymerTest.clearBody();
      testElement = document.createElement('nux-set-as-default');
      document.body.appendChild(testElement);
    });

    teardown(function() {
      testElement.remove();
    });

    test('skip', function() {
      testElement.$['decline-button'].click();
      return testSetAsDefaultProxy.whenCalled('recordSkip');
    });

    test(
        'click set-default button and finishes setting default',
        async function() {
          testElement.$$('.action-button').click();

          await Promise.all([
            testSetAsDefaultProxy.whenCalled('recordBeginSetDefault'),
            testSetAsDefaultProxy.whenCalled('setAsDefault'),
          ]);

          const notifyPromise =
              test_util.eventToPromise('default-browser-change', testElement);

          cr.webUIListenerCallback(
              'browser-default-state-changed', {isDefault: true});

          return Promise.all([
            notifyPromise,
            testSetAsDefaultProxy.whenCalled('recordSuccessfullySetDefault'),
            navigatedPromise
          ]);
        });

    test('click set-default button but gives up and skip', async function() {
      testElement.$$('.action-button').click();

      await Promise.all([
        testSetAsDefaultProxy.whenCalled('recordBeginSetDefault'),
        testSetAsDefaultProxy.whenCalled('setAsDefault'),
      ]);

      testElement.$['decline-button'].click();

      return Promise.all([
        testSetAsDefaultProxy.whenCalled('recordSkip'),
        navigatedPromise,
      ]);
    });
  });
});
