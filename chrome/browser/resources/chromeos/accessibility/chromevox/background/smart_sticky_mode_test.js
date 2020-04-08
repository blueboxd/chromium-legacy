// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_next_e2e_test_base.js', '../testing/assert_additions.js'
]);

/**
 * Test fixture for SmartStickyMode.
 */
ChromeVoxSmartStickyModeTest = class extends ChromeVoxNextE2ETest {};

TEST_F('ChromeVoxSmartStickyModeTest', 'PossibleRangeTypes', function() {
  this.runWithLoadedTree(
      `
    <p>start</p>
    <input aria-controls="controls-target" type="text"></input>
    <textarea aria-activedescendant="active-descendant-target"></textarea>
    <div contenteditable><h3>hello</h3></div>
    <ul id="controls-target"><li>end</ul>
    <ul id="active-descendant-target"><li>end</ul>
  `,
      function(root) {
        const ssm = new SmartStickyMode();

        // Deregister from actual range changes.
        ChromeVoxState.removeObserver(ssm);
        assertFalse(ssm.didTurnOffStickyMode_);
        const [p, input, textarea, contenteditable, ul1, ul2] = root.children;

        function assertDidTurnOffForNode(node) {
          ssm.onCurrentRangeChanged(cursors.Range.fromNode(node));
          assertTrue(ssm.didTurnOffStickyMode_);
        }

        function assertDidNotTurnOffForNode(node) {
          ssm.onCurrentRangeChanged(cursors.Range.fromNode(node));
          assertFalse(ssm.didTurnOffStickyMode_);
        }

        // First, turn on sticky mode and try changing range to various parts of
        // the document.
        ChromeVoxBackground.setPref(
            'sticky', true /* value */, true /* announce */);

        assertDidTurnOffForNode(input);
        assertDidTurnOffForNode(textarea);
        assertDidNotTurnOffForNode(p);

        assertDidTurnOffForNode(contenteditable);
        assertDidTurnOffForNode(ul1);
        assertDidNotTurnOffForNode(p);
        assertDidTurnOffForNode(ul2);
        assertDidTurnOffForNode(ul1.firstChild);
        assertDidNotTurnOffForNode(p);
        assertDidTurnOffForNode(ul2.firstChild);
        assertDidNotTurnOffForNode(p);
        assertDidTurnOffForNode(contenteditable.find({role: 'heading'}));
        assertDidTurnOffForNode(contenteditable.find({role: 'inlineTextBox'}));
      });
});
