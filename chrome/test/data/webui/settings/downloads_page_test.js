// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://settings/settings.js';
// #import {DownloadsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

/** @implements {settings.DownloadsBrowserProxy} */
class TestDownloadsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initializeDownloads',
      'selectDownloadLocation',
      'resetAutoOpenFileTypes',
      'getDownloadLocationText',
    ]);
  }

  /** @override */
  initializeDownloads() {
    this.methodCalled('initializeDownloads');
  }

  /** @override */
  selectDownloadLocation() {
    this.methodCalled('selectDownloadLocation');
  }

  /** @override */
  resetAutoOpenFileTypes() {
    this.methodCalled('resetAutoOpenFileTypes');
  }
}

suite('DownloadsHandler', function() {
  let downloadsBrowserProxy = null;
  let downloadsPage = null;

  setup(function() {
    downloadsBrowserProxy = new TestDownloadsBrowserProxy();
    settings.DownloadsBrowserProxyImpl.instance_ = downloadsBrowserProxy;

    PolymerTest.clearBody();

    downloadsPage = document.createElement('settings-downloads-page');
    document.body.appendChild(downloadsPage);

    // Page element must call 'initializeDownloads' upon attachment to the DOM.
    return downloadsBrowserProxy.whenCalled('initializeDownloads');
  });

  teardown(function() {
    downloadsPage.remove();
  });

  test('select downloads location', function() {
    const button = downloadsPage.$$('#changeDownloadsPath');
    assertTrue(!!button);
    button.click();
    button.fire('transitionend');
    return downloadsBrowserProxy.whenCalled('selectDownloadLocation');
  });

  test('openAdvancedDownloadsettings', function() {
    let button = downloadsPage.$$('#resetAutoOpenFileTypes');
    assertTrue(!button);

    cr.webUIListenerCallback('auto-open-downloads-changed', true);
    Polymer.dom.flush();
    button = downloadsPage.$$('#resetAutoOpenFileTypes');
    assertTrue(!!button);

    button.click();
    return downloadsBrowserProxy.whenCalled('resetAutoOpenFileTypes')
        .then(function() {
          cr.webUIListenerCallback('auto-open-downloads-changed', false);
          Polymer.dom.flush();
          const button = downloadsPage.$$('#resetAutoOpenFileTypes');
          assertTrue(!button);
        });
  });

  if (cr.isChromeOS) {
    /** @override */
    TestDownloadsBrowserProxy.prototype.getDownloadLocationText = function(
        path) {
      this.methodCalled('getDownloadLocationText', path);
      return Promise.resolve('downloads-text');
    };

    function setDefaultDownloadPathPref(downloadPath) {
      downloadsPage.prefs = {
        download: {
          default_directory: {
            key: 'download.default_directory',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: downloadPath,
          }
        }
      };
    }

    function getDefaultDownloadPathString() {
      const pathElement = downloadsPage.$$('#defaultDownloadPath');
      assertTrue(!!pathElement);
      return pathElement.textContent.trim();
    }

    test('rewrite default download paths', function() {
      setDefaultDownloadPathPref('downloads-path');
      return downloadsBrowserProxy.whenCalled('getDownloadLocationText')
          .then(path => {
            assertEquals('downloads-path', path);
            Polymer.dom.flush();
            assertEquals('downloads-text', getDefaultDownloadPathString());
          });
    });
  }
});
