// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-toolbar. */

// clang-format off
// #import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.m.js';
//
// #import {assertFalse, assertTrue} from '../chai_assert.js';
// clang-format on

suite('cr-toolbar', function() {
  /** @type {?CrToolbarElement} */
  let toolbar = null;

  setup(function() {
    document.body.innerHTML = '';
    toolbar =
        /** @type {!CrToolbarElement} */ (document.createElement('cr-toolbar'));
    document.body.appendChild(toolbar);
  });

  test('autofocus propagated to search field', () => {
    assertFalse(toolbar.autofocus);
    assertFalse(toolbar.getSearchField().hasAttribute('autofocus'));

    toolbar.autofocus = true;
    assertTrue(toolbar.getSearchField().hasAttribute('autofocus'));
  });
});
