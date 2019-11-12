// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bookmarks', function() {
  /**
   * Test version of the bookmarks browser proxy.
   */
  class TestBookmarksBrowserProxy extends TestBrowserProxy {
    constructor() {
      super(['getIncognitoAvailability', 'getCanEditBookmarks']);
    }

    getIncognitoAvailability() {
      this.methodCalled('getIncognitoAvailability');
      return Promise.resolve(IncognitoAvailability.DISABLED);
    }

    getCanEditBookmarks() {
      this.methodCalled('getCanEditBookmarks');
      return Promise.resolve(false);
    }

    getPluralString(messageName, itemCount) {
      return Promise.resolve('test');
    }

    recordInHistogram(histogram, bucket, maxBucket) {}
  }

  return {
    TestBookmarksBrowserProxy: TestBookmarksBrowserProxy,
  };
});
