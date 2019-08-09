// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the extensions error console. */
suite('CrExtensionsErrorConsoleTest', function() {
  const STACK_ERRORS = 'li';
  const ACTIVE_ERROR_IN_STACK = 'li[tabindex="0"]';

  // Initialize an extension activity log item before each test.
  setup(function() {
    return test_util.waitBeforeNextRender(document);
  });

  test('TestUpDownErrors', function() {
    let initialFocus =
        extension_test_util.findMatches(document, ACTIVE_ERROR_IN_STACK)[0];
    assertTrue(!!initialFocus);
    assertEquals(
        1,
        extension_test_util.findMatches(document, ACTIVE_ERROR_IN_STACK)
            .length);
    assertEquals(
        4, extension_test_util.findMatches(document, STACK_ERRORS).length);

    // Pressing up when the first item is focused should NOT change focus.
    MockInteractions.keyDownOn(initialFocus, 38, '', 'ArrowUp');
    assertEquals(
        initialFocus,
        extension_test_util.findMatches(document, ACTIVE_ERROR_IN_STACK)[0]);

    // Pressing down when the first item is focused should change focus.
    MockInteractions.keyDownOn(initialFocus, 40, '', 'ArrowDown');
    assertNotEquals(
        initialFocus,
        extension_test_util.findMatches(document, ACTIVE_ERROR_IN_STACK)[0]);

    // Pressing up when the second item is focused should focus the first again.
    MockInteractions.keyDownOn(initialFocus, 38, '', 'ArrowUp');
    assertEquals(
        initialFocus,
        extension_test_util.findMatches(document, ACTIVE_ERROR_IN_STACK)[0]);
  });
});
