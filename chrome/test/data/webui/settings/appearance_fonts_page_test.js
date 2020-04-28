// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FontsBrowserProxy, FontsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// clang-format on

/** @implements {FontsBrowserProxy} */
class TestFontsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'fetchFontsData',
    ]);

    /** @private {!FontsData} */
    this.fontsData_ = {
      'fontList': [['font name', 'alternate', 'ltr']],
      'encodingList': [['encoding name', 'alternate', 'ltr']],
    };
  }

  /** @override */
  fetchFontsData() {
    this.methodCalled('fetchFontsData');
    return Promise.resolve(this.fontsData_);
  }
}

let fontsPage = null;

/** @type {?TestFontsBrowserProxy} */
let fontsBrowserProxy = null;

suite('AppearanceFontHandler', function() {
  setup(function() {
    fontsBrowserProxy = new TestFontsBrowserProxy();
    FontsBrowserProxyImpl.instance_ = fontsBrowserProxy;

    PolymerTest.clearBody();

    fontsPage = document.createElement('settings-appearance-fonts-page');
    document.body.appendChild(fontsPage);
  });

  teardown(function() {
    fontsPage.remove();
  });

  test('fetchFontsData', function() {
    return fontsBrowserProxy.whenCalled('fetchFontsData');
  });

  test('minimum font size sample', async () => {
    fontsPage.prefs = {webkit: {webprefs: {minimum_font_size: {value: 0}}}};
    assertTrue(fontsPage.$.minimumSizeSample.hidden);
    fontsPage.set('prefs.webkit.webprefs.minimum_font_size.value', 6);
    assertFalse(fontsPage.$.minimumSizeSample.hidden);
    fontsPage.set('prefs.webkit.webprefs.minimum_font_size.value', 0);
    assertTrue(fontsPage.$.minimumSizeSample.hidden);
  });
});
