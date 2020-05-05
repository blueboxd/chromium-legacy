// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** This class handles navigation amongst the elements onscreen. */
class NavigationManager {
  /**
   * @param {!chrome.automation.AutomationNode} desktop
   * @private
   */
  constructor(desktop) {
    /** @private {!chrome.automation.AutomationNode} */
    this.desktop_ = desktop;

    /** @private {!SARootNode} */
    this.group_ = DesktopNode.build(this.desktop_);

    /** @private {!SAChildNode} */
    this.node_ = this.group_.firstChild;

    /** @private {!FocusHistory} */
    this.history_ = new FocusHistory();

    /** @private {!FocusRingManager} */
    this.focusRingManager_ = new FocusRingManager();

    /**
     * Callback for testing use only.
     * @private {?function()}
     */
    this.onMoveForwardForTesting_ = null;

    this.init_();
  }

  // =============== Static Methods ==============

  /**
   * Enters |this.node_|.
   */
  static enterGroup() {
    const navigator = NavigationManager.instance;
    if (!navigator.node_.isGroup()) {
      return;
    }

    SwitchAccessMetrics.recordMenuAction('EnterGroup');

    const newGroup = navigator.node_.asRootNode();
    if (newGroup) {
      navigator.history_.save(new FocusData(navigator.group_, navigator.node_));
      navigator.setGroup_(newGroup);
    }
  }

  /**
   * Puts focus on the virtual keyboard, if the current node is a text input.
   * TODO(crbug/946190): Handle the case where the user has not enabled the
   *     onscreen keyboard.
   */
  static enterKeyboard() {
    const navigator = NavigationManager.instance;
    const keyboard = KeyboardRootNode.buildTree();
    navigator.jumpTo_(keyboard);
    navigator.node_.automationNode.focus();
  }

  /**
   * Open the Switch Access menu for the currently highlighted node. If there
   * are not enough actions available to trigger the menu, the current element
   * is selected.
   */
  static enterMenu() {
    const navigator = NavigationManager.instance;
    const didEnter = MenuManager.enter(navigator.node_);

    // If the menu does not or cannot open, select the current node.
    if (!didEnter) {
      navigator.selectCurrentNode();
    }
  }

  static exitKeyboard() {
    const navigator = NavigationManager.instance;
    const isKeyboard = (data) => data.group instanceof KeyboardRootNode;
    // If we are not in the keyboard, do nothing.
    if (!(navigator.group_ instanceof KeyboardRootNode) &&
        !navigator.history_.containsDataMatchingPredicate(isKeyboard)) {
      return;
    }

    while (navigator.history_.peek() !== null) {
      if (navigator.group_ instanceof KeyboardRootNode) {
        navigator.exitGroup_();
        break;
      }
      navigator.exitGroup_();
    }

    NavigationManager.moveToValidNode();
  }

  /**
   * Forces the current node to be |node|.
   * Should only be called by subclasses of SARootNode and
   *    only when they are focused.
   * @param {!SAChildNode} node
   */
  static forceFocusedNode(node) {
    const navigator = NavigationManager.instance;
    navigator.setNode_(node);
  }

  /**
   * Returns the current Switch Access tree, for debugging purposes.
   * @param {boolean} wholeTree Whether to print the whole tree, or just the
   * current focus.
   * @return {!SARootNode}
   */
  static getTreeForDebugging(wholeTree) {
    if (!wholeTree) {
      console.log(NavigationManager.instance.group_.debugString(wholeTree));
      return NavigationManager.instance.group_;
    }

    const desktopRoot = DesktopNode.build(NavigationManager.instance.desktop_);
    console.log(desktopRoot.debugString(
        wholeTree, '', NavigationManager.instance.node_));
    return desktopRoot;
  }

  /** @param {!chrome.automation.AutomationNode} desktop */
  static initialize(desktop) {
    NavigationManager.instance = new NavigationManager(desktop);
  }

  /**
   * Move to the previous interesting node.
   */
  static moveBackward() {
    const navigator = NavigationManager.instance;

    if (MenuManager.moveBackward()) {
      // The menu navigation is handled separately. If we are in the menu, do
      // not change the primary focus node.
      return;
    }

    navigator.setNode_(navigator.node_.previous);
  }

  /**
   * Move to the next interesting node.
   */
  static moveForward() {
    const navigator = NavigationManager.instance;

    if (navigator.onMoveForwardForTesting_) {
      navigator.onMoveForwardForTesting_();
    }

    if (MenuManager.moveForward()) {
      // The menu navigation is handled separately. If we are in the menu, do
      // not change the primary focus node.
      return;
    }

    navigator.setNode_(navigator.node_.next);
  }

  /**
   * Moves to the Switch Access focus up the group stack closest to the ancestor
   * that hasn't been invalidated.
   */
  static moveToValidNode() {
    const navigator = NavigationManager.instance;

    const nodeIsValid = navigator.node_.isValidAndVisible();
    const groupIsValid = navigator.group_.isValidGroup();

    if (nodeIsValid && groupIsValid) {
      return;
    }

    if (nodeIsValid) {
      // Our group has been invalidated. Move to navigator node to repair the
      // group stack.
      const node = navigator.node_.automationNode;
      if (node) {
        navigator.moveTo_(node);
        return;
      }
    }

    if (groupIsValid) {
      navigator.setNode_(navigator.group_.firstChild);
      return;
    }

    navigator.restoreFromHistory_();
  }

  /**
   * Updates the focus ring locations in response to an automation event.
   */
  static refreshFocusRings() {
    const navigator = NavigationManager.instance;

    navigator.focusRingManager_.setFocusNodes(
        navigator.node_, navigator.group_);
  }

  /**
   * Returns the desktop automation node object.
   * @return {!chrome.automation.AutomationNode}
   */
  static get desktopNode() {
    return NavigationManager.instance.desktop_;
  }

  // =============== Instance Methods ==============

  /**
   * Selects the current node.
   */
  selectCurrentNode() {
    if (MenuManager.selectCurrentNode()) {
      // The menu navigation is handled separately. If we are in the menu, do
      // not change the primary focus node.
      return;
    }

    if (this.node_.isGroup()) {
      NavigationManager.enterGroup();
      return;
    }

    if (this.node_.hasAction(SAConstants.MenuAction.OPEN_KEYBOARD)) {
      SwitchAccessMetrics.recordMenuAction(
          SAConstants.MenuAction.OPEN_KEYBOARD);
      this.node_.performAction(SAConstants.MenuAction.OPEN_KEYBOARD);
      return;
    }

    if (this.node_.hasAction(SAConstants.MenuAction.SELECT)) {
      SwitchAccessMetrics.recordMenuAction(SAConstants.MenuAction.SELECT);
      this.node_.performAction(SAConstants.MenuAction.SELECT);
    }
  }

  // =============== Event Handlers ==============

  /**
   * Sets up the connection between the menuPanel and menuManager.
   * @param {!PanelInterface} menuPanel
   */
  connectMenuPanel(menuPanel) {
    menuPanel.backButtonElement().addEventListener(
        'click', this.exitGroup_.bind(this));
  }

  /**
   * When focus shifts, move to the element. Find the closest interesting
   *     element to engage with.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onFocusChange_(event) {
    if (this.node_.isEquivalentTo(event.target)) {
      return;
    }
    this.moveTo_(event.target);
  }

  /**
   * When a menu is opened, jump focus to the menu.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onMenuStart_(event) {
    const menuRoot = SystemMenuRootNode.buildTree(event.target);
    this.jumpTo_(menuRoot);
  }

  /**
   * When the automation tree changes, check if it affects any nodes we are
   *     currently listening to.
   * @param {!chrome.automation.TreeChange} treeChange
   * @private
   */
  onTreeChange_(treeChange) {
    if (treeChange.type === chrome.automation.TreeChangeType.NODE_REMOVED) {
      NavigationManager.moveToValidNode();
    }
  }

  // =============== Private Methods ==============

  /**
   * Exits the current group.
   * @private
   */
  exitGroup_() {
    this.group_.onExit();
    this.restoreFromHistory_();
  }

  /** @private */
  init_() {
    this.group_.onFocus();
    this.node_.onFocus();

    if (window.menuPanel) {
      this.connectMenuPanel(window.menuPanel);
    }

    this.desktop_.addEventListener(
        chrome.automation.EventType.FOCUS, this.onFocusChange_.bind(this),
        false);

    this.desktop_.addEventListener(
        chrome.automation.EventType.MENU_START, this.onMenuStart_.bind(this),
        false);

    chrome.automation.addTreeChangeObserver(
        chrome.automation.TreeChangeObserverFilter.ALL_TREE_CHANGES,
        this.onTreeChange_.bind(this));
  }

  /**
   * Jumps Switch Access focus to a specified node, such as when opening a menu
   * or the keyboard. Does not modify the groups already in the group stack.
   * @param {!SARootNode} group
   * @private
   */
  jumpTo_(group) {
    MenuManager.exit();

    this.history_.save(new FocusData(this.group_, this.node_));
    this.setGroup_(group);
  }

  /**
   * Moves Switch Access focus to a specified node, based on a focus shift or
   *     tree change event. Reconstructs the group stack to center on that node.
   *
   * This is a "permanent" move, while |jumpTo_| is a "temporary" move.
   *
   * @param {!chrome.automation.AutomationNode} automationNode
   * @private
   */
  moveTo_(automationNode) {
    MenuManager.exit();
    this.history_.buildFromAutomationNode(automationNode);
    if (!this.history_.peek().focus.isEquivalentTo(automationNode)) {
    }
    this.restoreFromHistory_();
  }

  /**
   * Restores the most proximal state from the history.
   * @private
   */
  restoreFromHistory_() {
    const data = this.history_.retrieve();
    // retrieve() guarantees that the group is valid, but not the focus.
    if (data.focus.isValidAndVisible()) {
      this.setGroup_(data.group, false /* shouldSetNode */);
      this.setNode_(data.focus);
    } else {
      this.setGroup_(data.group, true /* shouldSetNode */);
    }
  }

  /**
   * Set |this.group_| to |group|.
   * @param {!SARootNode} group
   * @param {boolean} shouldSetNode
   * @private
   */
  setGroup_(group, shouldSetNode = true) {
    this.group_.onUnfocus();
    this.group_ = group;
    this.group_.onFocus();

    if (shouldSetNode) {
      this.setNode_(this.group_.firstChild);
    }
  }

  /**
   * Set |this.node_| to |node|, and update what is displayed onscreen.
   * @param {!SAChildNode} node
   * @private
   */
  setNode_(node) {
    this.node_.onUnfocus();
    this.node_ = node;
    this.node_.onFocus();
    this.focusRingManager_.setFocusNodes(this.node_, this.group_);
    AutoScanManager.restartIfRunning();
  }
}
