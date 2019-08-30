// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * NavigatorDelegate for calling browser-specific functions to do the actual
 * navigating.
 */
class NavigatorDelegate {
  /**
   * @param {number} tabId The tab ID of the PDF viewer or -1 if the viewer is
   *     not displayed in a tab.
   */
  constructor(tabId) {
    /** @private {number} */
    this.tabId_ = tabId;
  }

  /**
   * Called when navigation should happen in the current tab.
   * @param {string} url The url to be opened in the current tab.
   */
  navigateInCurrentTab(url) {
    // When the PDFviewer is inside a browser tab, prefer the tabs API because
    // it can navigate from one file:// URL to another.
    if (chrome.tabs && this.tabId_ != -1) {
      chrome.tabs.update(this.tabId_, {url: url});
    } else {
      window.location.href = url;
    }
  }

  /**
   * Called when navigation should happen in the new tab.
   * @param {string} url The url to be opened in the new tab.
   * @param {boolean} active Indicates if the new tab should be the active tab.
   */
  navigateInNewTab(url, active) {
    // Prefer the tabs API because it guarantees we can just open a new tab.
    // window.open doesn't have this guarantee.
    if (chrome.tabs) {
      chrome.tabs.create({url: url, active: active});
    } else {
      window.open(url);
    }
  }

  /**
   * Called when navigation should happen in the new window.
   * @param {string} url The url to be opened in the new window.
   */
  navigateInNewWindow(url) {
    // Prefer the windows API because it guarantees we can just open a new
    // window. window.open with '_blank' argument doesn't have this guarantee.
    if (chrome.windows) {
      chrome.windows.create({url: url});
    } else {
      window.open(url, '_blank');
    }
  }
}

/** Navigator for navigating to links inside or outside the PDF. */
class PdfNavigator {
  /**
   * @param {string} originalUrl The original page URL.
   * @param {!Viewport} viewport The viewport info of the page.
   * @param {!OpenPdfParamsParser} paramsParser The object for URL parsing.
   * @param {!NavigatorDelegate} navigatorDelegate The object with callback
   *    functions that get called when navigation happens in the current tab,
   *    a new tab, and a new window.
   */
  constructor(originalUrl, viewport, paramsParser, navigatorDelegate) {
    this.originalUrl_ = originalUrl;
    this.viewport_ = viewport;
    this.paramsParser_ = paramsParser;
    this.navigatorDelegate_ = navigatorDelegate;
  }

  /**
   * Function to navigate to the given URL. This might involve navigating
   * within the PDF page or opening a new url (in the same tab or a new tab).
   * @param {string} url The URL to navigate to.
   * @param {!PdfNavigator.WindowOpenDisposition} disposition The window open
   *     disposition when navigating to the new URL.
   */
  navigate(url, disposition) {
    if (url.length == 0) {
      return;
    }

    // If |urlFragment| starts with '#', then it's for the same URL with a
    // different URL fragment.
    if (url.charAt(0) == '#') {
      // if '#' is already present in |originalUrl| then remove old fragment
      // and add new url fragment.
      const hashIndex = this.originalUrl_.search('#');
      if (hashIndex != -1) {
        url = this.originalUrl_.substring(0, hashIndex) + url;
      } else {
        url = this.originalUrl_ + url;
      }
    }

    // If there's no scheme, then take a guess at the scheme.
    if (url.indexOf('://') == -1 && url.indexOf('mailto:') == -1) {
      url = this.guessUrlWithoutScheme_(url);
    }

    if (!this.isValidUrl_(url)) {
      return;
    }

    switch (disposition) {
      case PdfNavigator.WindowOpenDisposition.CURRENT_TAB:
        this.paramsParser_.getViewportFromUrlParams(
            url, this.onViewportReceived_.bind(this));
        break;
      case PdfNavigator.WindowOpenDisposition.NEW_BACKGROUND_TAB:
        this.navigatorDelegate_.navigateInNewTab(url, false);
        break;
      case PdfNavigator.WindowOpenDisposition.NEW_FOREGROUND_TAB:
        this.navigatorDelegate_.navigateInNewTab(url, true);
        break;
      case PdfNavigator.WindowOpenDisposition.NEW_WINDOW:
        this.navigatorDelegate_.navigateInNewWindow(url);
        break;
      case PdfNavigator.WindowOpenDisposition.SAVE_TO_DISK:
        // TODO(jaepark): Alt + left clicking a link in PDF should
        // download the link.
        this.paramsParser_.getViewportFromUrlParams(
            url, this.onViewportReceived_.bind(this));
        break;
      default:
        break;
    }
  }

  /**
   * Called when the viewport position is received.
   * @param {Object} viewportPosition Dictionary containing the viewport
   *    position.
   * @private
   */
  onViewportReceived_(viewportPosition) {
    let originalUrl = this.originalUrl_;
    let hashIndex = originalUrl.search('#');
    if (hashIndex != -1) {
      originalUrl = originalUrl.substring(0, hashIndex);
    }

    let newUrl = viewportPosition.url;
    hashIndex = newUrl.search('#');
    if (hashIndex != -1) {
      newUrl = newUrl.substring(0, hashIndex);
    }

    const pageNumber = viewportPosition.page;
    if (pageNumber != undefined && originalUrl == newUrl) {
      this.viewport_.goToPage(pageNumber);
    } else {
      this.navigatorDelegate_.navigateInCurrentTab(viewportPosition.url);
    }
  }

  /**
   * Checks if the URL starts with a scheme and is not just a scheme.
   * TODO (rbpotter): Update to use URL (here and elsewhere in this file).
   * @param {string} url The input URL
   * @return {boolean} Whether the url is valid.
   * @private
   */
  isValidUrl_(url) {
    // Make sure |url| starts with a valid scheme.
    if (!url.startsWith('http://') && !url.startsWith('https://') &&
        !url.startsWith('ftp://') && !url.startsWith('file://') &&
        !url.startsWith('mailto:')) {
      return false;
    }

    // Navigations to file:-URLs are only allowed from file:-URLs.
    if (url.startsWith('file:') && !this.originalUrl_.startsWith('file:')) {
      return false;
    }


    // Make sure |url| is not only a scheme.
    if (url == 'http://' || url == 'https://' || url == 'ftp://' ||
        url == 'file://' || url == 'mailto:') {
      return false;
    }

    return true;
  }

  /**
   * Attempt to figure out what a URL is when there is no scheme.
   * @param {string} url The input URL
   * @return {string} The URL with a scheme or the original URL if it is not
   *     possible to determine the scheme.
   * @private
   */
  guessUrlWithoutScheme_(url) {
    // If the original URL is mailto:, that does not make sense to start with,
    // and neither does adding |url| to it.
    // If the original URL is not a valid URL, this cannot make a valid URL.
    // In both cases, just bail out.
    if (this.originalUrl_.startsWith('mailto:') ||
        !this.isValidUrl_(this.originalUrl_)) {
      return url;
    }

    // Check for absolute paths.
    if (url.startsWith('/')) {
      const schemeEndIndex = this.originalUrl_.indexOf('://');
      const firstSlash = this.originalUrl_.indexOf('/', schemeEndIndex + 3);
      // e.g. http://www.foo.com/bar -> http://www.foo.com
      const domain = firstSlash != -1 ?
          this.originalUrl_.substr(0, firstSlash) :
          this.originalUrl_;
      return domain + url;
    }

    // Check for obvious relative paths.
    let isRelative = false;
    if (url.startsWith('.') || url.startsWith('\\')) {
      isRelative = true;
    }

    // In Adobe Acrobat Reader XI, it looks as though links with less than
    // 2 dot separators in the domain are considered relative links, and
    // those with 2 of more are considered http URLs. e.g.
    //
    // www.foo.com/bar -> http
    // foo.com/bar -> relative link
    if (!isRelative) {
      const domainSeparatorIndex = url.indexOf('/');
      const domainName = domainSeparatorIndex == -1 ?
          url :
          url.substr(0, domainSeparatorIndex);
      const domainDotCount = (domainName.match(/\./g) || []).length;
      if (domainDotCount < 2) {
        isRelative = true;
      }
    }

    if (isRelative) {
      const slashIndex = this.originalUrl_.lastIndexOf('/');
      const path = slashIndex != -1 ? this.originalUrl_.substr(0, slashIndex) :
                                      this.originalUrl_;
      return path + '/' + url;
    }

    return 'http://' + url;
  }
}

/**
 * Represents options when navigating to a new url. C++ counterpart of
 * the enum is in ui/base/window_open_disposition.h. This enum represents
 * the only values that are passed from Plugin.
 * @enum {number}
 */
PdfNavigator.WindowOpenDisposition = {
  CURRENT_TAB: 1,
  NEW_FOREGROUND_TAB: 3,
  NEW_BACKGROUND_TAB: 4,
  NEW_WINDOW: 6,
  SAVE_TO_DISK: 7
};
