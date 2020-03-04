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

    if (SwitchAccess.instance.improvedTextInputEnabled() &&
        this.automationNode.state[StateType.FOCUSED]) {
      actions.push(SAConstants.MenuAction.MOVE_CURSOR);
      actions.push(SAConstants.MenuAction.SELECT_START);
      if (TextNavigationManager.currentlySelecting()) {
        actions.push(SAConstants.MenuAction.SELECT_END);
      }
      if (TextNavigationManager.selectionExists) {
        actions.push(SAConstants.MenuAction.CUT);
        actions.push(SAConstants.MenuAction.COPY);
      }
      if (TextNavigationManager.clipboardHasData) {
        actions.push(SAConstants.MenuAction.PASTE);
      }
    }

    return actions;
  }

  // ================= General methods =================

  /** @override */
  performAction(action) {
    switch (action) {
      case SAConstants.MenuAction.OPEN_KEYBOARD:
        NavigationManager.enterKeyboard();
        return SAConstants.ActionResponse.CLOSE_MENU;
      case SAConstants.MenuAction.DICTATION:
        chrome.accessibilityPrivate.toggleDictation();
        return SAConstants.ActionResponse.CLOSE_MENU;
      case SAConstants.MenuAction.CUT:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.X, {ctrl: true});
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SAConstants.MenuAction.COPY:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.C, {ctrl: true});
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SAConstants.MenuAction.PASTE:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.V, {ctrl: true});
        return SAConstants.ActionResponse.REMAIN_OPEN;
    }
    return super.performAction(action);
  }
}
