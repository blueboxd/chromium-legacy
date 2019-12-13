// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

export class TestProxy {
  constructor() {
    /** @type {newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter = new newTabPage.mojom.PageCallbackRouter();

    /** @type {!newTabPage.mojom.PageRemote} */
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    /** @type {newTabPage.mojom.PageHandlerInterface} */
    this.handler = new FakePageHandler(this.callbackRouterRemote);
  }
}

/** @implements {newTabPage.mojom.PageHandlerInterface} */
class FakePageHandler {
  /** @param {newTabPage.mojom.PageInterface} */
  constructor(callbackRouterRemote) {
    /** @private {newTabPage.mojom.PageInterface} */
    this.callbackRouterRemote_ = callbackRouterRemote;

    /** @private {TestBrowserProxy} */
    this.callTracker_ = new TestBrowserProxy([
      'addMostVisitedTile',
      'deleteMostVisitedTile',
      'reorderMostVisitedTile',
      'restoreMostVisitedDefaults',
      'undoMostVisitedTileAction',
      'updateMostVisitedTile',
    ]);
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.callTracker_.whenCalled(methodName);
  }

  /** @override */
  addMostVisitedTile(url, title) {
    this.callTracker_.methodCalled('addMostVisitedTile', [url, title]);
    return {success: true};
  }

  /** @override */
  deleteMostVisitedTile(url) {
    this.callTracker_.methodCalled('deleteMostVisitedTile', url);
    return {success: true};
  }

  /** @override */
  reorderMostVisitedTile(url, newPos) {
    this.callTracker_.methodCalled('reorderMostVisitedTile', [url, newPos]);
  }

  /** @override */
  restoreMostVisitedDefaults() {
    this.callTracker_.methodCalled('restoreMostVisitedDefaults');
  }

  /** @override */
  undoMostVisitedTileAction() {
    this.callTracker_.methodCalled('undoMostVisitedTileAction');
  }

  /** @override */
  updateMostVisitedTile(url, newUrl, newTitle) {
    this.callTracker_.methodCalled(
        'updateMostVisitedTile', [url, newUrl, newTitle]);
    return {success: true};
  }
}

/**
 * @param {!HTMLElement} element
 * @param {string} key
 */
export function keydown(element, key) {
  element.dispatchEvent(new KeyboardEvent('keydown', {key: key}));
}
