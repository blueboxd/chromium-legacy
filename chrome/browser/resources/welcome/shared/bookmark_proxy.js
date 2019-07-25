// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('welcome', function() {
  /**
   * @typedef {{
   *    parentId: string,
   *    title: string,
   *    url: string,
   * }}
   */
  let bookmarkData;

  /** @interface */
  class BookmarkProxy {
    /**
     * @param {!bookmarkData} data
     * @param {!Function} callback
     */
    addBookmark(data, callback) {}

    /** @param {string} id ID provided by callback when bookmark was added. */
    removeBookmark(id) {}

    /** @param {boolean} show */
    toggleBookmarkBar(show) {}

    /** @return {!Promise<boolean>} */
    isBookmarkBarShown() {}
  }

  /** @implements {welcome.BookmarkProxy} */
  class BookmarkProxyImpl {
    /** @override */
    addBookmark(data, callback) {
      chrome.bookmarks.create(data, callback);
    }

    /** @override */
    removeBookmark(id) {
      chrome.bookmarks.remove(id);
    }

    /** @override */
    toggleBookmarkBar(show) {
      chrome.send('toggleBookmarkBar', [show]);
    }

    /** @override */
    isBookmarkBarShown() {
      return cr.sendWithPromise('isBookmarkBarShown');
    }
  }

  cr.addSingletonGetter(BookmarkProxyImpl);

  // Wrapper for bookmark proxy to keep some additional states.
  class BookmarkBarManager {
    constructor() {
      /** @private {welcome.BookmarkProxy} */
      this.proxy_ = BookmarkProxyImpl.getInstance();

      /** @private {boolean} */
      this.isBarShown_ = false;

      /** @type {!Promise} */
      this.initialized = this.proxy_.isBookmarkBarShown().then(shown => {
        this.isBarShown_ = shown;
      });
    }

    /** @return {boolean} */
    getShown() {
      return this.isBarShown_;
    }

    /** @param {boolean} show */
    setShown(show) {
      this.isBarShown_ = show;
      this.proxy_.toggleBookmarkBar(show);
    }
  }

  cr.addSingletonGetter(BookmarkBarManager);

  return {
    BookmarkProxy: BookmarkProxy,
    BookmarkProxyImpl: BookmarkProxyImpl,
    BookmarkBarManager: BookmarkBarManager,
  };
});
