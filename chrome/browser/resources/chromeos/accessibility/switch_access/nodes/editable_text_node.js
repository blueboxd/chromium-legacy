// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles interactions with editable text fields.
 */
class EditableTextNode extends NodeWrapper {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   */
  constructor(baseNode, parent) {
    super(baseNode, parent);
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    const actions = super.actions;
    // The SELECT action is used to press buttons, etc. For text inputs, the
    // equivalent action is OPEN_KEYBOARD, which focuses the input and opens the
    // keyboard.
    const selectIndex = actions.indexOf(SAConstants.MenuAction.SELECT);
    if (selectIndex >= 0) {
      actions.splice(selectIndex, 1);
    }

    actions.push(SAConstants.MenuAction.OPEN_KEYBOARD);
    actions.push(SAConstants.MenuAction.DICTATION);

    return actions;
  }

  // ================= General methods =================

  /** @override */
  performAction(action) {
    switch (action) {
      case SAConstants.MenuAction.OPEN_KEYBOARD:
        this.automationNode.focus();
        return SAConstants.ActionResponse.CLOSE_MENU;
      case SAConstants.MenuAction.DICTATION:
        chrome.accessibilityPrivate.toggleDictation();
        return SAConstants.ActionResponse.CLOSE_MENU;
    }
    return super.performAction(action);
  }
}
