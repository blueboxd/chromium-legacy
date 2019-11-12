// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bookmarks', function() {
  'use strict';

  class BrowserProxy {
    /**
     * @return {!Promise<!IncognitoAvailability>} Promise resolved with the
     *     current incognito mode preference.
     */
    getIncognitoAvailability() {
      return cr.sendWithPromise('getIncognitoAvailability');
    }

    /**
     * @return {!Promise<boolean>} Promise resolved with whether the bookmarks
     *     can be edited.
     */
    getCanEditBookmarks() {
      return cr.sendWithPromise('getCanEditBookmarks');
    }

    /**
     * @return {!Promise<string>} Promise resolved with the appropriate plural
     *     string for |messageName| with |itemCount| items.
     */
    getPluralString(messageName, itemCount) {
      return cr.sendWithPromise('getPluralString', messageName, itemCount);
    }

    /**
     * Notifies the metrics handler to record a histogram value.
     * @param {string} histogram The name of the histogram to record
     * @param {number} bucket The bucket to record
     * @param {number} maxBucket The maximum bucket value in the histogram.
     */
    recordInHistogram(histogram, bucket, maxBucket) {
      chrome.send(
          'metricsHandler:recordInHistogram', [histogram, bucket, maxBucket]);
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {
    BrowserProxy: BrowserProxy,
  };
});
