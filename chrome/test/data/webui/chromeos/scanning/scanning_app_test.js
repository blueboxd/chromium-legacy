// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jschettler): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://scanning/scanning_app.js';

suite('ScanningAppTest', () => {
  /** @type {?ScanningAppElement} */
  let page = null;

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('scanning-app');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  test('MainPageLoaded', () => {
    // TODO(jschettler): Remove this stub test once the page has more
    // capabilities to test.
    assertEquals('Chrome OS Scanning', page.$$('#header').textContent);
  });
});
