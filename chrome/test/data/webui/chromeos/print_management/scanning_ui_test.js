// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jschettler): use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://scanning/scanning_ui.js';

suite('ScanningUITest', () => {
  /** @type {?ScanningUIElement} */
  let page = null;

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('scanning-ui');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  test('MainPageLoaded', () => {
    // TODO(jschettler): Remove this stub test once the UI has more capabilities
    // to test.
    assertEquals('Scanning UI', page.$$('#header').textContent);
  });
});
