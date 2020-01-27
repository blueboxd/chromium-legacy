// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for NodeIdentifier.
 * @constructor
 * @extends {ChromeVoxE2ETest}
 */
function ChromeVoxNodeIdentifierTest() {
  ChromeVoxNextE2ETest.call(this);
}

ChromeVoxNodeIdentifierTest.prototype = {
  __proto__: ChromeVoxNextE2ETest.prototype,

  /**
   * Returns the start node of the current ChromeVox range.
   */
  getRangeStart: function() {
    return ChromeVoxState.instance.getCurrentRange().start.node;
  },

  /** @override */
  setUp: function() {
    window.RoleType = chrome.automation.RoleType;
  },

  // Test documents //

  basicButtonDoc: `
    <p>Start here</p>
    <button id="apple-button">Apple</button>
    <button>Orange</button>
  `,

  duplicateButtonDoc: `
    <p>Start here</p>
    <button>Click me</button>
    <button>Click me</button>
  `,

  identicalListsDoc: `
    <p>Start here</p>
    <ul>
      <li>Apple</li>
      <li>Orange</li>
    </ul>
    <ul>
      <li>Apple</li>
      <li>Orange</li>
    </ul>
  `
};

// Tests that we can distinguish between two similar buttons.
TEST_F('ChromeVoxNodeIdentifierTest', 'BasicButtonTest', function() {
  this.runWithLoadedTree(this.basicButtonDoc, function(rootNode) {
    var appleNode =
        rootNode.find({role: RoleType.BUTTON, attributes: {name: 'Apple'}});
    var orangeNode =
        rootNode.find({role: RoleType.BUTTON, attributes: {name: 'Orange'}});
    assertFalse(!appleNode);
    assertFalse(!orangeNode);
    var appleId = new NodeIdentifier(appleNode);
    var duplicateAppleId = new NodeIdentifier(appleNode);
    var orangeId = new NodeIdentifier(orangeNode);

    assertTrue(appleId.equals(duplicateAppleId));
    assertFalse(appleId.equals(orangeId));
  });
});

// Tests that we can distinguish two buttons with the same name.
TEST_F('ChromeVoxNodeIdentifierTest', 'DuplicateButtonTest', function() {
  this.runWithLoadedTree(this.duplicateButtonDoc, function() {
    CommandHandler.onCommand('nextButton');
    var firstButton = this.getRangeStart();
    var firstButtonId = new NodeIdentifier(firstButton);
    CommandHandler.onCommand('nextButton');
    var secondButton = this.getRangeStart();
    var secondButtonId = new NodeIdentifier(secondButton);

    assertFalse(firstButtonId.equals(secondButtonId));
  });
});

// Tests that we can differentiate between the list items of two identical
// lists.
TEST_F('ChromeVoxNodeIdentifierTest', 'IdenticalListsTest', function() {
  this.runWithLoadedTree(this.identicalListsDoc, function() {
    // Create NodeIdentifiers for each item.
    CommandHandler.onCommand('nextObject');
    CommandHandler.onCommand('nextObject');
    var firstApple = new NodeIdentifier(this.getRangeStart());
    CommandHandler.onCommand('nextObject');
    CommandHandler.onCommand('nextObject');
    var firstOrange = new NodeIdentifier(this.getRangeStart());
    CommandHandler.onCommand('nextObject');
    CommandHandler.onCommand('nextObject');
    var secondApple = new NodeIdentifier(this.getRangeStart());
    CommandHandler.onCommand('nextObject');
    CommandHandler.onCommand('nextObject');
    var secondOrange = new NodeIdentifier(this.getRangeStart());

    assertFalse(firstApple.equals(secondApple));
    assertFalse(firstOrange.equals(secondOrange));
  });
});
