// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): use es6 module for mojo binding crbug/1004256
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://print-management/print_management.js';

suite('PrintManagementTest', () => {
  /** @type {?PrintManagementElement} */
  let page = null;

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('print-management');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  test('MainPageLoaded', () => {
    // TODO(jimmyxgong): Remove this stub test once the app has more
    // capabilities to test.
    assertEquals('Print Management', page.$$('#header').textContent);
  });
});