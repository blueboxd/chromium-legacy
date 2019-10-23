// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This interface represents some object or group of objects on screen
 *     that Switch Access may be interested in interacting with.
 *
 * There is no guarantee of uniqueness; two distinct SAChildNodes may refer
 *     to the same object. However, it is expected that any pair of
 *     SAChildNodes referring to the same interesting object are equal
 *     (calling .equals() returns true).
 * @abstract
 */
class SAChildNode {
  /**
   * @param {boolean} isGroup
   */
  constructor(isGroup) {
    this.isGroup_ = isGroup;

    /** @private {?SAChildNode} */
    this.previous_ = null;

    /** @private {?SAChildNode} */
    this.next_ = null;
  }

  /**
   * @param {!SAChildNode} previous
   * @protected
   */
  setPrevious_(previous) {
    this.previous_ = previous;
  }

  /**
   * @param {!SAChildNode} next
   * @protected
   */
  setNext_(next) {
    this.next_ = next;
  }

  /**
   * @param {SAChildNode} other
   * @return {boolean}
   * @abstract
   */
  equals(other) {}

  /**
   * @return {chrome.automation.RoleType|undefined}
   * @abstract
   */
  get role() {}

  /**
   * @return {chrome.accessibilityPrivate.ScreenRect|undefined}
   * @abstract
   */
  get location() {}

  /**
   * Returns the underlying automation node, if one exists.
   * @return {chrome.automation.AutomationNode}
   * @abstract
   */
  get automationNode() {}

  /**
   * Returns whether this node should be displayed as a group.
   * @return {boolean}
   */
  isGroup() {
    return this.isGroup_;
  }

  /**
   * Returns a list of all the actions available for this node.
   * @return {!Array<SAConstants.MenuAction>}
   * @abstract
   */
  get actions() {}

  /**
   * Given a menu action, returns whether it can be performed on this node.
   * @param {SAConstants.MenuAction} action
   * @return {boolean}
   */
  hasAction(action) {
    return this.actions.includes(action);
  }

  /**
   * Performs the specified action on the node, if it is available.
   * @param {SAConstants.MenuAction} action
   * @return {boolean} Whether to close the menu. True if the menu should close,
   *     false otherwise.
   * @abstract
   */
  performAction(action) {}

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   * @abstract
   */
  isEquivalentTo(node) {}

  /**
   * Returns the next node in pre-order traversal.
   * @return {!SAChildNode}
   */
  get next() {
    if (!this.next_) {
      throw new Error(
          'Next node must be set on all SAChildNodes before navigating');
    }
    return this.next_;
  }

  /**
   * Returns the previous node in pre-order traversal.
   * @return {!SAChildNode}
   */
  get previous() {
    if (!this.previous_) {
      throw new Error(
          'Previous node must be set on all SAChildNodes before navigating');
    }
    return this.previous_;
  }

  /**
   * If this node is a group, returns the analogous SARootNode.
   * @return {SARootNode}
   * @abstract
   */
  asRootNode() {}

  /**
   * Called when a node becomes the primary highlighted node.
   */
  onFocus() {}

  /**
   * Called when a node stops being the primary highlighted node.
   */
  onUnfocus() {}
}

/**
 * This class represents the root node of a Switch Access traversal group.
 */
class SARootNode {
  constructor() {
    /** @private {!Array<!SAChildNode>} */
    this.children_ = [];
  }

  /**
   * @param {!Array<!SAChildNode>} children
   * @protected
   */
  setChildren_(children) {
    this.children_ = children;
  }

  /**
   * @param {SARootNode} other
   * @return {boolean}
   */
  equals(other) {
    if (!other) return false;
    if (this.children_.length !== other.children_.length) return false;

    let result = true;
    for (let i = 0; i < this.children_.length; i++) {
      if (!this.children_[i]) throw new Error('Child cannot be null.');
      result = result && this.children_[i].equals(other.children_[i]);
    }

    return result;
  }

  /** @return {!Array<!SAChildNode>} */
  get children() {
    return this.children_;
  }

  /** @return {!SAChildNode} */
  get firstChild() {
    if (this.children_.length > 0) {
      return this.children_[0];
    } else {
      throw new Error('Root nodes must contain children.');
    }
  }

  /** @return {!SAChildNode} */
  get lastChild() {
    if (this.children_.length > 0) {
      return this.children_[this.children_.length - 1];
    } else {
      throw new Error('Root nodes must contain children.');
    }
  }

  /** @return {boolean} */
  isValid() {
    return true;
  }

  /** @return {!chrome.accessibilityPrivate.ScreenRect} */
  get location() {
    let childLocations = this.children_.map((c) => c.location);
    return RectHelper.unionAll(childLocations);
  }

  /**
   * @param {AutomationNode} automationNode
   * @return {boolean}
   */
  isEquivalentTo(automationNode) {
    return false;
  }

  /** @return {AutomationNode} */
  get automationNode() {}

  /** Called when a group is exiting. */
  onExit() {}

  /**
   * Helper function to connect children.
   * @param {!Array<!SAChildNode>} children
   */
  static connectChildren(children) {
    if (children.length < 1) {
      throw new Error('Root node must have at least 1 interesting child.');
    }

    let previous = children[children.length - 1];

    for (let i = 0; i < children.length; i++) {
      let current = children[i];
      previous.setNext_(current);
      current.setPrevious_(previous);

      previous = current;
    }
  }
}
