// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles interactions with the desktop automation node.
 */
class DesktopNode extends RootNodeWrapper {
  /**
   * @param {!AutomationNode} autoNode The automation node representing the
   *     desktop.
   */
  constructor(autoNode) {
    super(autoNode);
  }

  // ================= General methods =================

  /** @override */
  equals(other) {
    // The underlying automation tree only has one desktop node, so all
    // DesktopNode instances are equal.
    return other instanceof DesktopNode;
  }

  /** @override */
  isValidGroup() {
    return true;
  }

  /** @override */
  refresh() {
    // Find the currently focused child.
    let focusedChild = null;
    for (const child of this.children) {
      if (child.isFocused()) {
        focusedChild = child;
        break;
      }
    }

    // Update this DesktopNode's children.
    const childConstructor = (node) => NodeWrapper.create(node, this);
    DesktopNode.findAndSetChildren(this, childConstructor);

    // Set the new instance of that child to be the focused node.
    for (const child of this.children) {
      if (child.isEquivalentTo(focusedChild)) {
        NavigationManager.forceFocusedNode(child);
        return;
      }
    }

    // If the previously focused node no longer exists, focus the first node in
    // the group.
    NavigationManager.forceFocusedNode(this.children[0]);
  }

  // ================= Static methods =================

  /**
   * @param {!AutomationNode} desktop
   * @return {!DesktopNode}
   */
  static build(desktop) {
    const root = new DesktopNode(desktop);
    const childConstructor = (autoNode) => NodeWrapper.create(autoNode, root);

    DesktopNode.findAndSetChildren(root, childConstructor);
    return root;
  }

  /** @override */
  static findAndSetChildren(root, childConstructor) {
    const interestingChildren = RootNodeWrapper.getInterestingChildren(root);

    if (interestingChildren.length < 1) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.MALFORMED_DESKTOP,
          'Desktop node must have at least 1 interesting child.');
    }

    root.children = interestingChildren.map(childConstructor);
  }
}
