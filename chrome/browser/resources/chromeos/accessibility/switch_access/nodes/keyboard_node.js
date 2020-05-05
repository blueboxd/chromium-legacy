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

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    if (this.isGroup()) {
      return [];
    }
    return [SAConstants.MenuAction.SELECT];
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    if (!this.isGroup()) {
      return null;
    }

    const node = this.automationNode;
    if (!node) {
      throw new TypeError('Keyboard nodes must have an automation node.');
    }

    const root = new RootNodeWrapper(node);
    KeyboardNode.findAndSetChildren(root);
    return root;
  }

  /** @override */
  performAction(action) {
    if (this.isGroup() || action !== SAConstants.MenuAction.SELECT) {
      return SAConstants.ActionResponse.NO_ACTION_TAKEN;
    }

    const keyLocation = this.location;
    if (!keyLocation) {
      return SAConstants.ActionResponse.NO_ACTION_TAKEN;
    }

    // doDefault() does nothing on Virtual Keyboard buttons, so we must
    // simulate a mouse click.
    const center = RectHelper.center(keyLocation);
    EventHelper.simulateMouseClick(
        center.x, center.y, SAConstants.VK_KEY_PRESS_DURATION_MS);

    return SAConstants.ActionResponse.CLOSE_MENU;
  }

  // ================= Static methods =================

  /**
   * Helper function to connect tree elements, given the root node.
   * @param {!RootNodeWrapper} root
   */
  static findAndSetChildren(root) {
    const childConstructor = (node) => new KeyboardNode(node, root);

    /** @type {!Array<!chrome.automation.AutomationNode>} */
    const interestingChildren = RootNodeWrapper.getInterestingChildren(root);
    let children = interestingChildren.map(childConstructor);
    if (interestingChildren.length > SAConstants.KEYBOARD_MAX_ROW_LENGTH) {
      children = GroupNode.separateByRow(children);
    }

    children.push(new BackButtonNode(root));
    root.children = children;
  }
}

/**
 * This class handles the top-level Keyboard node, as well as the construction
 * of the Keyboard tree.
 */
class KeyboardRootNode extends RootNodeWrapper {
  /**
   * @param {!chrome.automation.AutomationNode} groupNode
   * @private
   */
  constructor(groupNode) {
    super(groupNode);
  }

  // ================= General methods =================


  /** @override */
  isValidGroup() {
    // To ensure we can find the keyboard root node to appropriately respond to
    // visibility changes, never mark it as invalid.
    return true;
  }

  /** @override */
  onExit() {
    // If the keyboard is currently visible, ignore the corresponding
    // state change.
    if (KeyboardRootNode.isVisible_) {
      KeyboardRootNode.explicitStateChange_ = true;
      chrome.accessibilityPrivate.setVirtualKeyboardVisible(false);
    }

    AutoScanManager.setInKeyboard(false);
  }

  // ================= Static methods =================

  /**
   * Creates the tree structure for the system menu.
   * @return {!KeyboardRootNode}
   */
  static buildTree() {
    KeyboardRootNode.loadKeyboard_();
    AutoScanManager.setInKeyboard(true);

    if (!KeyboardRootNode.keyboardObject_) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.MISSING_KEYBOARD,
          'Could not find keyboard in the automation tree');
    }
    const keyboard =
        new AutomationTreeWalker(
            KeyboardRootNode.keyboardObject_, constants.Dir.FORWARD, {
              visit: (node) => SwitchAccessPredicate.isGroup(node, null),
              root: (node) => node === KeyboardRootNode.keyboardObject_
            })
            .next()
            .node;

    const root = new KeyboardRootNode(keyboard);
    KeyboardNode.findAndSetChildren(root);
    return root;
  }

  /**
   * Start listening for keyboard open/closed.
   */
  static startWatchingVisibility() {
    KeyboardRootNode.isVisible_ =
        SwitchAccessPredicate.isVisible(KeyboardRootNode.keyboardObject_);

    KeyboardRootNode.keyboardObject_.addEventListener(
        chrome.automation.EventType.ARIA_ATTRIBUTE_CHANGED,
        KeyboardRootNode.checkVisibilityChanged_, false /* capture */);
  }

  // ================= Private static methods =================

  /**
   * @param {chrome.automation.AutomationEvent} event
   * @private
   */
  static checkVisibilityChanged_(event) {
    const currentlyVisible =
        SwitchAccessPredicate.isVisible(KeyboardRootNode.keyboardObject_);
    if (currentlyVisible === KeyboardRootNode.isVisible_) {
      return;
    }

    KeyboardRootNode.isVisible_ = currentlyVisible;

    if (KeyboardRootNode.explicitStateChange_) {
      // When the user has explicitly shown / hidden the keyboard, do not
      // enter / exit the keyboard again to avoid looping / double-calls.
      KeyboardRootNode.explicitStateChange_ = false;
      return;
    }

    if (KeyboardRootNode.isVisible_) {
      NavigationManager.enterKeyboard();
    } else {
      NavigationManager.exitKeyboard();
    }
  }

  /**
   * @return {chrome.automation.AutomationNode}
   * @private
   */
  static get keyboardObject_() {
    if (!this.object_ || !this.object_.role) {
      this.object_ = NavigationManager.desktopNode.find(
          {role: chrome.automation.RoleType.KEYBOARD});
    }
    return this.object_;
  }

  /**
   * Loads the keyboard.
   * @private
   */
  static loadKeyboard_() {
    if (KeyboardRootNode.isVisible_) {
      return;
    }

    KeyboardRootNode.explicitStateChange_ = true;
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(true);
  }
}
