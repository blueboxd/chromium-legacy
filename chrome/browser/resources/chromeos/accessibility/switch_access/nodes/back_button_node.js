// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles the behavior of the back button.
 */
class BackButtonNode extends SAChildNode {
  /**
   * @param {!SARootNode} group
   */
  constructor(group) {
    super();
    /**
     * The group that the back button is shown for.
     * @private {!SARootNode}
     */
    this.group_ = group;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [SAConstants.MenuAction.SELECT];
  }

  /** @override */
  get automationNode() {
    return BackButtonNode.automationNode;
  }

  /** @override */
  get location() {
    if (BackButtonNode.locationForTesting) {
      return BackButtonNode.locationForTesting;
    }
    if (this.automationNode) {
      return this.automationNode.location;
    }
  }

  /** @override */
  get role() {
    return chrome.automation.RoleType.BUTTON;
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    return null;
  }

  /** @override */
  equals(other) {
    return other instanceof BackButtonNode;
  }

  /** @override */
  isEquivalentTo(node) {
    return node instanceof BackButtonNode || this.automationNode === node;
  }

  /** @override */
  isGroup() {
    return false;
  }

  /** @override */
  isValidAndVisible() {
    return this.automationNode !== null;
  }

  /** @override */
  onFocus() {
    super.onFocus();
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        true, this.group_.location, 0 /* num_actions */);
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        false, RectHelper.ZERO_RECT, 0 /* num_actions */);
  }

  /** @override */
  performAction(action) {
    if (action === SAConstants.MenuAction.SELECT &&
        BackButtonNode.automationNode_) {
      BackButtonNode.automationNode_.doDefault();
      return SAConstants.ActionResponse.CLOSE_MENU;
    }
    return SAConstants.ActionResponse.NO_ACTION_TAKEN;
  }

  // ================= Debug methods =================

  /** @override */
  debugString() {
    return 'BackButtonNode';
  }

  // ================= Static methods =================

  /**
   * Looks for the back button node.
   * @return {?chrome.automation.AutomationNode}
   */
  static get automationNode() {
    if (BackButtonNode.automationNode_) {
      return BackButtonNode.automationNode_;
    }

    const treeWalker = new AutomationTreeWalker(
        NavigationManager.desktopNode, constants.Dir.FORWARD,
        {visit: (node) => node.htmlAttributes.id === SAConstants.BACK_ID});
    BackButtonNode.automationNode_ = treeWalker.next().node;
    return BackButtonNode.automationNode_;
  }
}
