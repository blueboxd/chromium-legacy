// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles the behavior of keyboard nodes directly associated with a
 * single AutomationNode.
 */
class KeyboardNode extends NodeWrapper {
  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {!SARootNode} parent
   */
  constructor(node, parent) {
    super(node, parent);
  }

  /** @override */
  get actions() {
    if (this.isGroup()) return [];
    return [SAConstants.MenuAction.SELECT];
  }

  /** @override */
  performAction(action) {
    if (this.isGroup()) return false;
    if (action !== SAConstants.MenuAction.SELECT) return false;
    let keyLocation = this.location;
    if (!keyLocation) return false;

    // doDefault() does nothing on Virtual Keyboard buttons, so we must
    // simulate a mouse click.
    const center = RectHelper.center(keyLocation);
    EventHelper.simulateMouseClick(
        center.x, center.y, SAConstants.VK_KEY_PRESS_DURATION_MS);

    return true;
  }

  /** @override */
  asRootNode() {
    if (!this.isGroup()) return null;

    const node = this.automationNode;
    if (!node) {
      throw new TypeError('Keyboard nodes must have an automation node.');
    }

    return KeyboardNode.buildTree_(node);
  }

  /**
   * Builds a tree of KeyboardNodes.
   * @param {!chrome.automation.AutomationNode} automationNode
   * @return {!SARootNode}
   * @private
   */
  static buildTree_(automationNode) {
    const root = new RootNodeWrapper(automationNode);
    const childConstructor = (node) => new KeyboardNode(node, root);

    RootNodeWrapper.buildHelper(root, childConstructor);
    return root;
  }
}

/**
 * This class handles the top-level Keyboard node, as well as the construction
 * of the Keyboard tree.
 */
class KeyboardRootNode extends RootNodeWrapper {
  /**
   * @param {!chrome.automation.AutomationNode} keyboard
   * @private
   */
  constructor(keyboard) {
    super(keyboard);
  }

  /** @override */
  onExit() {
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(false);
  }

  /**
   * Custom logic when entering the node.
   */
  onEnter_() {
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(true);
  }

  /**
   * Creates the tree structure for the system menu.
   * @param {!chrome.automation.AutomationNode} desktop
   * @return {!KeyboardRootNode}
   */
  static buildTree(desktop) {
    const keyboardContainer =
        desktop.find({role: chrome.automation.RoleType.KEYBOARD});
    const keyboard =
        new AutomationTreeWalker(keyboardContainer, constants.Dir.FORWARD, {
          visit: (node) => SwitchAccessPredicate.isGroup(node, null),
          root: (node) => node === keyboardContainer
        })
            .next()
            .node;

    const root = new KeyboardRootNode(keyboard);
    root.onEnter_();
    const childConstructor = (node) => new KeyboardNode(node, root);

    RootNodeWrapper.buildHelper(root, childConstructor);
    return root;
  }
}
