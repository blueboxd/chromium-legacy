// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationNode = chrome.automation.AutomationNode;

/**
 * This class handles interactions with an onscreen element based on a single
 * AutomationNode.
 */
class NodeWrapper extends SAChildNode {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   */
  constructor(baseNode, parent) {
    super(SwitchAccessPredicate.isGroup(baseNode, parent));
    /** @private {!AutomationNode} */
    this.baseNode_ = baseNode;
  }

  /** @override */
  equals(other) {
    if (!other || !(other instanceof NodeWrapper)) return false;

    other = /** @type {!NodeWrapper} */ (other);
    return other.baseNode_ === this.baseNode_;
  }

  /** @override */
  get role() {
    return this.baseNode_.role;
  }

  /** @override */
  get location() {
    return this.baseNode_.location;
  }

  /** @override */
  get automationNode() {
    return this.baseNode_;
  }

  /** @override */
  get actions() {
    let actions = [];
    if (SwitchAccessPredicate.isTextInput(this.baseNode_)) {
      actions.push(SAConstants.MenuAction.OPEN_KEYBOARD);
      actions.push(SAConstants.MenuAction.DICTATION);
    } else {
      actions.push(SAConstants.MenuAction.SELECT);
    }

    const ancestor = this.getScrollableAncestor_();
    if (ancestor.scrollable) {
      if (ancestor.scrollX > ancestor.scrollXMin) {
        actions.push(SAConstants.MenuAction.SCROLL_LEFT);
      }
      if (ancestor.scrollX < ancestor.scrollXMax) {
        actions.push(SAConstants.MenuAction.SCROLL_RIGHT);
      }
      if (ancestor.scrollY > ancestor.scrollYMin) {
        actions.push(SAConstants.MenuAction.SCROLL_UP);
      }
      if (ancestor.scrollY < ancestor.scrollYMax) {
        actions.push(SAConstants.MenuAction.SCROLL_DOWN);
      }
    }
    const standardActions = /** @type {!Array<!SAConstants.MenuAction>} */ (
        this.baseNode_.standardActions.filter(
            action => Object.values(SAConstants.MenuAction).includes(action)));

    return actions.concat(standardActions);
  }

  /** @override */
  performAction(action) {
    let ancestor;
    switch (action) {
      case SAConstants.MenuAction.OPEN_KEYBOARD:
        this.baseNode_.focus();
        return true;
      case SAConstants.MenuAction.SELECT:
        this.baseNode_.doDefault();
        return true;
      case SAConstants.MenuAction.DICTATION:
        chrome.accessibilityPrivate.toggleDictation();
        return true;
      case SAConstants.MenuAction.SCROLL_DOWN:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) ancestor.scrollDown(() => {});
        return true;
      case SAConstants.MenuAction.SCROLL_UP:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) ancestor.scrollUp(() => {});
        return true;
      case SAConstants.MenuAction.SCROLL_RIGHT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) ancestor.scrollRight(() => {});
        return true;
      case SAConstants.MenuAction.SCROLL_LEFT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) ancestor.scrollLeft(() => {});
        return true;
      default:
        if (Object.values(chrome.automation.ActionType).includes(action)) {
          this.baseNode_.performStandardAction(
              /** @type {chrome.automation.ActionType} */ (action));
        }
        return true;
    }
  }

  /**
   * @return {AutomationNode}
   * @protected
   */
  getScrollableAncestor_() {
    let ancestor = this.baseNode_;
    while (!ancestor.scrollable && ancestor.parent)
      ancestor = ancestor.parent;
    return ancestor;
  }

  /** @override */
  isEquivalentTo(node) {
    return this.baseNode_ === node;
  }

  /** @override */
  asRootNode() {
    if (!this.isGroup()) return null;
    return RootNodeWrapper.buildTree(this.baseNode_);
  }
}

/**
 * This class handles constructing and traversing a group of onscreen elements
 * based on all the interesting descendants of a single AutomationNode.
 */
class RootNodeWrapper extends SARootNode {
  /**
   * @param {!AutomationNode} baseNode
   */
  constructor(baseNode) {
    super();

    /** @private {!AutomationNode} */
    this.baseNode_ = baseNode;
  }

  /** @override */
  equals(other) {
    if (!(other instanceof RootNodeWrapper)) return false;

    other = /** @type {!RootNodeWrapper} */ (other);
    return super.equals(other) && this.baseNode_ === other.baseNode_;
  }

  /** @override */
  get location() {
    return this.baseNode_.location || super.location;
  }

  /** @override */
  isValid() {
    return !!this.baseNode_.role;
  }

  /** @override */
  isEquivalentTo(automationNode) {
    return this.baseNode_ === automationNode;
  }

  /** @override */
  get automationNode() {
    return this.baseNode_;
  }

  /**
   * @param {!AutomationNode} rootNode
   * @return {!RootNodeWrapper}
   */
  static buildTree(rootNode) {
    const root = new RootNodeWrapper(rootNode);
    const childConstructor = (node) => new NodeWrapper(node, root);

    RootNodeWrapper.buildHelper(root, childConstructor);
    return root;
  }

  /**
   * Helper function to connect tree elements, given constructors for the root
   * and child types.
   * @param {!RootNodeWrapper} root
   * @param {function(!AutomationNode): !SAChildNode} childConstructor
   *     Constructs a child node from an automation node.
   */
  static buildHelper(root, childConstructor) {
    const interestingChildren = RootNodeWrapper.getInterestingChildren(root);

    if (interestingChildren.length < 1) {
      throw new Error('Root node must have at least 1 interesting child.');
    }

    const backButton = new BackButtonNode(root);

    let children = interestingChildren.map(childConstructor);
    children.push(backButton);

    SARootNode.connectChildren(children);
    root.setChildren_(children);

    return;
  }

  /**
   * @param {!AutomationNode} desktop
   * @return {!RootNodeWrapper}
   */
  static buildDesktopTree(desktop) {
    const root = new RootNodeWrapper(desktop);
    const interestingChildren = RootNodeWrapper.getInterestingChildren(root);

    if (interestingChildren.length < 1) {
      throw new Error('Desktop node must have at least 1 interesting child.');
    }

    const childConstructor = (autoNode) => new NodeWrapper(autoNode, root);
    let children = interestingChildren.map(childConstructor);

    SARootNode.connectChildren(children);
    root.setChildren_(children);

    return root;
  }

  /**
   * @param {!RootNodeWrapper} root
   * @return {!Array<!AutomationNode>}
   */
  static getInterestingChildren(root) {
    let interestingChildren = [];
    let treeWalker = new AutomationTreeWalker(
        root.baseNode_, constants.Dir.FORWARD,
        SwitchAccessPredicate.restrictions(root));
    let node = treeWalker.next().node;

    while (node) {
      interestingChildren.push(node);
      node = treeWalker.next().node;
    }

    return interestingChildren;
  }
}
