// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_next_e2e_test_base.js', '../testing/assert_additions.js'
]);

/**
 * Test fixture for ChromeVox KeyboardHandler.
 */
ChromeVoxBackgroundKeyboardHandlerTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    window.keyboardHandler = new BackgroundKeyboardHandler();
  }

  createMockKeyEvent(keyCode, modifiers) {
    const keyEvent = {};
    keyEvent.keyCode = keyCode;
    for (const key in modifiers) {
      keyEvent[key] = modifiers[key];
    }
    keyEvent.preventDefault = _ => {};
    keyEvent.stopPropagation = _ => {};
    return keyEvent;
  }
};


TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'SearchGetsPassedThrough',
    function() {
      this.runWithLoadedTree('<p>test</p>', function() {
        // A Search keydown gets eaten.
        const searchDown = {};
        searchDown.preventDefault = this.newCallback();
        searchDown.stopPropagation = this.newCallback();
        searchDown.metaKey = true;
        keyboardHandler.onKeyDown(searchDown);
        assertEquals(1, keyboardHandler.eatenKeyDowns_.size);

        // A Search keydown does not get eaten when there's no range.
        ChromeVoxState.instance.setCurrentRange(null);
        const searchDown2 = {};
        searchDown2.metaKey = true;
        keyboardHandler.onKeyDown(searchDown2);
        assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
      });
    });

TEST_F('ChromeVoxBackgroundKeyboardHandlerTest', 'PassThroughMode', function() {
  this.runWithLoadedTree('<p>test</p>', function() {
    assertUndefined(ChromeVox.passThroughMode);
    assertEquals(0, keyboardHandler.passThroughKeyUpCount_);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);

    // Send the pass through command: Search+Shift+Escape.
    const search = this.createMockKeyEvent(91, {metaKey: true});
    keyboardHandler.onKeyDown(search);
    assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
    assertUndefined(ChromeVox.passThroughMode);
    assertEquals(0, keyboardHandler.passThroughKeyUpCount_);

    const searchShift =
        this.createMockKeyEvent(16, {metaKey: true, shiftKey: true});
    keyboardHandler.onKeyDown(searchShift);
    assertEquals(2, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passThroughKeyUpCount_);
    assertUndefined(ChromeVox.passThroughMode);

    const searchShiftEsc =
        this.createMockKeyEvent(27, {metaKey: true, shiftKey: true});
    keyboardHandler.onKeyDown(searchShiftEsc);
    assertEquals(3, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passThroughKeyUpCount_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(searchShiftEsc);
    assertEquals(2, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(1, keyboardHandler.passThroughKeyUpCount_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(searchShift);
    assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(2, keyboardHandler.passThroughKeyUpCount_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(search);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(3, keyboardHandler.passThroughKeyUpCount_);
    assertTrue(ChromeVox.passThroughMode);

    // Now, the next series of key downs should be passed through.
    // Try Search+Ctrl+M.
    keyboardHandler.onKeyDown(search);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(3, keyboardHandler.passThroughKeyUpCount_);
    assertTrue(ChromeVox.passThroughMode);

    const searchCtrl =
        this.createMockKeyEvent(77, {metaKey: true, ctrlKey: true});
    keyboardHandler.onKeyDown(searchCtrl);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(3, keyboardHandler.passThroughKeyUpCount_);
    assertTrue(ChromeVox.passThroughMode);

    const searchCtrlM =
        this.createMockKeyEvent(77, {metaKey: true, ctrlKey: true});
    keyboardHandler.onKeyDown(searchCtrlM);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(3, keyboardHandler.passThroughKeyUpCount_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(searchCtrlM);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passThroughKeyUpCount_);
    assertFalse(ChromeVox.passThroughMode);
  });
});
