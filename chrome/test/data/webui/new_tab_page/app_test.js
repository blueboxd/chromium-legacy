// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/app.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {TestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageAppTest', () => {
  /** @type {!NewTabPageApp} */
  let app;

  /** @type {TestProxy} */
  let testProxy;

  /** @type {CrToastManagerElement} */
  let toastManager;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = new TestProxy();
    BrowserProxy.instance_ = testProxy;

    app = document.createElement('new-tab-page-app');
    document.body.appendChild(app);

    toastManager = app.shadowRoot.querySelector('cr-toast-manager');
  });

  test('toast restore defaults button', async () => {
    const wait = testProxy.handler.whenCalled('restoreMostVisitedDefaults');
    assertFalse(toastManager.isToastOpen);
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);
    app.$.restore.click();
    await wait;
    assertFalse(toastManager.isToastOpen);
  });

  test('toast undo button', async () => {
    const wait = testProxy.handler.whenCalled('undoMostVisitedTileAction');
    assertFalse(toastManager.isToastOpen);
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);
    app.$.undo.click();
    await wait;
    assertFalse(toastManager.isToastOpen);
  });

  test('clicking customize button opens customize dialog', async () => {
    assertFalse(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
    app.$.customizeButton.click();
    await flushTasks();
    assertTrue(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });
});
