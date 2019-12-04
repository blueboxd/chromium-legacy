// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class TestBrowserService extends TestBrowserProxy {
  constructor() {
    super([
      'deleteForeignSession',
      'historyLoaded',
      'navigateToUrl',
      'openForeignSessionTab',
      'otherDevicesInitialized',
      'recordHistogram',
      'removeVisits',
      'queryHistory',
    ]);
    this.histogramMap = {};
    this.actionMap = {};
    /** @private {?PromiseResolver} */
    this.delayedRemove_ = null;
  }

  delayDelete() {
    this.delayedRemove_ = new PromiseResolver();
  }

  /** @override */
  deleteForeignSession(sessionTag) {
    this.methodCalled('deleteForeignSession', sessionTag);
  }

  /** @override */
  removeVisits(visits) {
    this.methodCalled('removeVisits', visits);
    if (this.delayedRemove_) {
      return this.delayedRemove_.promise;
    }
    return Promise.resolve();
  }

  finishRemoveVisits() {
    this.delayedRemove_.resolve();
    this.delayedRemove_ = null;
  }

  /** @override */
  historyLoaded() {
    this.methodCalled('historyLoaded');
  }

  /** @override */
  menuPromoShown() {}

  /** @override */
  navigateToUrl(url, target, e) {
    this.methodCalled('navigateToUrl', url);
  }

  /** @override */
  openClearBrowsingData() {}

  /** @override */
  openForeignSessionAllTabs() {}

  /** @override */
  openForeignSessionTab(sessionTag, windowId, tabId, e) {
    this.methodCalled('openForeignSessionTab', {
      sessionTag: sessionTag,
      windowId: windowId,
      tabId: tabId,
      e: e,
    });
  }

  /** @override */
  otherDevicesInitialized() {
    this.methodCalled('otherDevicesInitialized');
  }

  /** @override */
  queryHistory(searchTerm) {
    this.methodCalled('queryHistory', searchTerm);
  }

  /** @override */
  queryHistoryContinuation() {}

  /** @override */
  recordAction(action) {
    if (!(action in this.actionMap)) {
      this.actionMap[action] = 0;
    }

    this.actionMap[action]++;
  }

  /** @override */
  recordHistogram(histogram, value, max) {
    assertTrue(value <= max);

    if (!(histogram in this.histogramMap)) {
      this.histogramMap[histogram] = {};
    }

    if (!(value in this.histogramMap[histogram])) {
      this.histogramMap[histogram][value] = 0;
    }

    this.histogramMap[histogram][value]++;
    this.methodCalled('recordHistogram');
  }

  /** @override */
  recordTime() {}

  /** @override */
  removeBookmark() {}
}
