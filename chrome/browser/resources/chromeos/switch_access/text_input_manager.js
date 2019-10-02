// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle text input for improved accuracy and efficiency.
 */

class TextInputManager {
  /** @param {!NavigationManager} navigationManager */
  constructor(navigationManager) {
    /** @private {chrome.automation.AutomationNode} */
    this.node_;

    /** @private {!NavigationManager} */
    this.navigationManager_ = navigationManager;
  }

  /**
   * Enters the keyboard and draws the text input focus ring.
   * @param {!chrome.automation.AutomationNode} node
   */
  enterKeyboard(node) {
    if (!SwitchAccessPredicate.isTextInput(node))
      return false;

    this.node_ = node;
    this.navigationManager_.focusRingManager.setRing(
        SAConstants.Focus.ID.TEXT, [this.node_.location]);
    return true;
  }

  /** Resets the focus ring and the focus in |navigationManager_|. */
  returnToTextFocus() {
    if (!this.node_)
      return;
    this.navigationManager_.focusRingManager.clearRing(
        SAConstants.Focus.ID.TEXT);
    this.navigationManager_.exitCurrentScope(this.node_);
    this.node_ = null;
  }

  /**
   * Sends a keyEvent to click the center of the provided node.
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean} Whether a key was pressed.
   */
  pressKey(node) {
    if (node.role !== chrome.automation.RoleType.BUTTON)
      return false;
    if (!this.inVirtualKeyboard(node))
      return false;

    const x = node.location.left + Math.round(node.location.width / 2);
    const y = node.location.top + Math.round(node.location.height / 2);

    EventHelper.simulateMouseClick(x, y, SAConstants.VK_KEY_PRESS_DURATION_MS);
    return true;
  }

  /**
   * Returns the container with a role of keyboard.
   * @param {!chrome.automation.AutomationNode} desktop
   * @return {chrome.automation.AutomationNode}
   */
  getKeyboard(desktop) {
    let treeWalker = new AutomationTreeWalker(
        desktop, constants.Dir.FORWARD,
        {visit: (node) => node.role === chrome.automation.RoleType.KEYBOARD});
    const keyboardContainer = treeWalker.next().node;
    treeWalker =
        new AutomationTreeWalker(keyboardContainer, constants.Dir.FORWARD, {
          visit: (node) => SwitchAccessPredicate.isGroup(node, node),
          root: (node) => node === keyboardContainer
        });
    return treeWalker.next().node;
  }

  /**
   * Checks if |node| is in the virtual keyboard.
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   */
  inVirtualKeyboard(node) {
    if (node.role === chrome.automation.RoleType.KEYBOARD)
      return true;
    if (node.parent)
      return this.inVirtualKeyboard(node.parent);
    return false;
  }

  /**
   * Cuts currently selected text using a keyboard shortcut via synthetic keys.
   * If there's no selected text, doesn't do anything.
   * @public
   */
  cut() {
    EventHelper.simulateKeyPress(EventHelper.KeyCode.X, {ctrl: true});
  }

  /**
   * Copies currently selected text using a keyboard shortcut via synthetic
   * keys. If there's no selected text, doesn't do anything.
   * @public
   */
  copy() {
    EventHelper.simulateKeyPress(EventHelper.KeyCode.C, {ctrl: true});
  }

  /**
   * Pastes text from the clipboard using a keyboard shortcut via synthetic
   * keys. If there's nothing in the clipboard, doesn't do anything.
   * @public
   */
  paste() {
    EventHelper.simulateKeyPress(EventHelper.KeyCode.V, {ctrl: true});
  }
}
