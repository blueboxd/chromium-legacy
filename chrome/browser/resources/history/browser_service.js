// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a singleton object, history.BrowserService, which
 * provides access to chrome.send APIs.
 */

cr.define('history', function() {
  class BrowserService {
    historyLoaded() {
      chrome.send('historyLoaded');
    }

    /**
     * @param {!string} url
     */
    removeBookmark(url) {
      chrome.send('removeBookmark', [url]);
    }

    /**
     * @param {!Array<!HistoryEntry>} removalList
     * @return {!Promise} Promise that is resolved when items are deleted
     *     successfully or rejected when deletion fails.
     */
    removeVisits(removalList) {
      return cr.sendWithPromise('removeVisits', removalList);
    }

    /**
     * @param {string} sessionTag
     */
    openForeignSessionAllTabs(sessionTag) {
      chrome.send('openForeignSession', [sessionTag]);
    }

    /**
     * @param {string} sessionTag
     * @param {number} windowId
     * @param {number} tabId
     * @param {MouseEvent} e
     */
    openForeignSessionTab(sessionTag, windowId, tabId, e) {
      chrome.send('openForeignSession', [
        sessionTag, String(windowId), String(tabId), e.button || 0, e.altKey,
        e.ctrlKey, e.metaKey, e.shiftKey
      ]);
    }

    /**
     * @param {string} sessionTag
     */
    deleteForeignSession(sessionTag) {
      chrome.send('deleteForeignSession', [sessionTag]);
    }

    openClearBrowsingData() {
      chrome.send('clearBrowsingData');
    }

    /**
     * @param {string} histogram
     * @param {number} value
     * @param {number} max
     */
    recordHistogram(histogram, value, max) {
      chrome.send('metricsHandler:recordInHistogram', [histogram, value, max]);
    }

    /**
     * Record an action in UMA.
     * @param {string} action The name of the action to be logged.
     */
    recordAction(action) {
      if (action.indexOf('_') == -1) {
        action = `HistoryPage_${action}`;
      }
      chrome.send('metricsHandler:recordAction', [action]);
    }

    /**
     * @param {string} histogram
     * @param {number} time
     */
    recordTime(histogram, time) {
      chrome.send('metricsHandler:recordTime', [histogram, time]);
    }

    menuPromoShown() {
      chrome.send('menuPromoShown');
    }

    /**
     * @param {string} url
     * @param {string} target
     * @param {!MouseEvent} e
     */
    navigateToUrl(url, target, e) {
      chrome.send(
          'navigateToUrl',
          [url, target, e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
    }

    otherDevicesInitialized() {
      chrome.send('otherDevicesInitialized');
    }

    queryHistoryContinuation() {
      chrome.send('queryHistoryContinuation');
    }

    /** @param {string} searchTerm */
    queryHistory(searchTerm) {
      chrome.send('queryHistory', [searchTerm, RESULTS_PER_PAGE]);
    }

    startSignInFlow() {
      chrome.send('startSignInFlow');
    }
  }

  cr.addSingletonGetter(BrowserService);

  return {BrowserService: BrowserService};
});
