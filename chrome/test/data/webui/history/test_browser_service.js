// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class TestBrowserService extends TestBrowserProxy {
  constructor() {
    super([
      'deleteForeignSession',
      'deleteItems',
      'historyLoaded',
      'navigateToUrl',
      'openForeignSessionTab',
      'otherDevicesInitialized',
      'recordHistogram',
      'queryHistory',
    ]);
    this.histogramMap = {};
    this.actionMap = {};
    this.pendingDeletePromise_ = null;
    this.deleted_ = null;
  }

  /** @override */
  deleteForeignSession(sessionTag) {
    this.methodCalled('deleteForeignSession', sessionTag);
  }

  /** @override */
  deleteItems(items) {
    this.deleted_ = items;
    this.pendingDeletePromise_ = new PromiseResolver();
    this.methodCalled('deleteItems', items.map(item => {
      return {
        url: item.url,
        timestamps: item.allTimestamps,
      };
    }));

    return this.pendingDeletePromise_.promise;
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

  /** @override */
  resolveDelete(successful) {
    if (successful) {
      this.pendingDeletePromise_.resolve(this.deleted_);
    } else {
      this.pendingDeletePromise_.reject(this.deleted_);
    }
  }
}
