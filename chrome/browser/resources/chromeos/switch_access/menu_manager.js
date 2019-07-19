// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle interactions with the Switch Access menu, including moving
 * through and selecting actions.
 */

class MenuManager {
  /**
   * @param {!NavigationManager} navigationManager
   * @param {!chrome.automation.AutomationNode} desktop
   */
  constructor(navigationManager, desktop) {
    /**
     * A list of the Menu actions that are currently enabled.
     * @private {!Array<!SAConstants.MenuAction>}
     */
    this.actions_ = [];

    /**
     * The parent automation manager.
     * @private {!NavigationManager}
     */
    this.navigationManager_ = navigationManager;

    /**
     * The root node of the screen.
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;

    /**
     * The root node of the menu.
     * @private {chrome.automation.AutomationNode}
     */
    this.menuNode_;

    /**
     * The current node of the menu.
     * @private {chrome.automation.AutomationNode}
     */
    this.node_;

    /**
     * Keeps track of when we're in the Switch Access menu.
     * @private {boolean}
     */
    this.inMenu_ = false;

    /**
     * A reference to the Switch Access Menu Panel class.
     * @private {PanelInterface}
     */
    this.menuPanel_;
  }

  /**
   * Enter the menu and highlight the first available action.
   *
   * @param {!chrome.automation.AutomationNode} navNode the currently selected
   *        node, for which the menu is to be displayed.
   */
  enter(navNode) {
    if (!this.menuPanel_) {
      console.log('Error: Menu panel has not loaded.');
      return;
    }

    const actions = this.getActionsForNode_(navNode);
    // getActionsForNode_ will return null when there is only one interesting
    // action (selection) specific to this node. In this case, rather than
    // forcing the user to repeatedly disambiguate, we will simply select by
    // default.
    if (actions === null) {
      this.navigationManager_.selectCurrentNode();
      return;
    }

    this.inMenu_ = true;
    if (actions !== this.actions_) {
      this.actions_ = actions;
      this.menuPanel_.setActions(this.actions_);
    }

    if (navNode.location) {
      chrome.accessibilityPrivate.setSwitchAccessMenuState(
          true, navNode.location, actions.length);
    } else {
      console.log('Unable to show Switch Access menu.');
    }

    let firstNode =
        this.menuNode().find({role: chrome.automation.RoleType.BUTTON});
    while (firstNode && !this.isActionAvailable_(firstNode.htmlAttributes.id))
      firstNode = firstNode.nextSibling;

    if (firstNode) {
      this.node_ = firstNode;
      this.updateFocusRing_();
    }
  }

  /**
   * Reload the menu.
   * @param {!chrome.automation.AutomationNode} navNode the node for which the
   * menu is to be displayed.
   * @private
   */
  reloadMenu_(navNode) {
    const actionNode = this.node_;

    if (!this.menuPanel_) {
      console.log('Error: Menu panel has not loaded.');
      return;
    }

    const actions = this.getActionsForNode_(navNode);
    if (actions === null) {
      return;
    }

    this.inMenu_ = true;
    if (actions !== this.actions_) {
      this.actions_ = actions;
      this.menuPanel_.setActions(this.actions_);
    }

    if (navNode.location) {
      chrome.accessibilityPrivate.setSwitchAccessMenuState(
          true /* show menu */, navNode.location, actions.length);
    } else {
      console.log('Unable to show Switch Access menu.');
    }

    if (actionNode) {
      this.node_ = actionNode;
      this.updateFocusRing_();
    }
  }

  /**
   * Exits the menu.
   */
  exit() {
    this.clearFocusRing_();
    this.inMenu_ = false;
    if (this.node_)
      this.node_ = null;

    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        false, SAConstants.EMPTY_LOCATION, 0);
  }

  /**
   * Move to the next available action in the menu. If this is no next action,
   * focus the whole menu to loop again.
   * @return {boolean} Whether this function had any effect.
   */
  moveForward() {
    this.calculateCurrentNode();

    if (!this.inMenu_ || !this.node_)
      return false;

    this.clearFocusRing_();
    const treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.FORWARD,
        SwitchAccessPredicate.restrictions(this.menuNode()));
    const node = treeWalker.next().node;
    if (!node)
      this.node_ = null;
    else
      this.node_ = node;
    this.updateFocusRing_();
    return true;
  }

  /**
   * Move to the previous available action in the menu. If we're at the
   * beginning of the list, start again at the end.
   * @return {boolean} Whether this function had any effect.
   */
  moveBackward() {
    this.calculateCurrentNode();

    if (!this.inMenu_ || !this.node_)
      return false;

    this.clearFocusRing_();
    const treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.BACKWARD,
        SwitchAccessPredicate.restrictions(this.menuNode()));
    let node = treeWalker.next().node;

    // If node is null, find the last enabled button.
    let lastChild = this.menuNode().lastChild;
    while (!node && lastChild) {
      if (SwitchAccessPredicate.isActionable(lastChild)) {
        node = lastChild;
        break;
      } else {
        lastChild = lastChild.previousSibling;
      }
    }

    this.node_ = node;
    this.updateFocusRing_();
    return true;
  }

  /**
   * Perform the action indicated by the current button (or no action if the
   * entire menu is selected). Then exit the menu and return to traditional
   * navigation.
   * @return {boolean} Whether this function had any effect.
   */
  selectCurrentNode() {
    this.calculateCurrentNode();

    if (!this.inMenu_ || !this.node_)
      return false;


    if (window.switchAccess.textEditingEnabled()) {
      if (this.node_.role == RoleType.BUTTON) {
        this.node_.doDefault();
      } else {
        this.exit();
      }
    } else {
      this.clearFocusRing_();
      this.node_.doDefault();
      this.exit();
    }

    return true;
  }

  /**
   * Sets up the connection between the menuPanel and the menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {!MenuManager}
   */
  connectMenuPanel(menuPanel) {
    this.menuPanel_ = menuPanel;
    return this;
  }

  /**
   * Get the menu node. If it's not defined, search for it.
   * @return {!chrome.automation.AutomationNode}
   */
  menuNode() {
    if (this.menuNode_)
      return this.menuNode_;

    const treeWalker = new AutomationTreeWalker(
        this.desktop_, constants.Dir.FORWARD,
        SwitchAccessPredicate.switchAccessMenuDiscoveryRestrictions());
    const node = treeWalker.next().node;
    if (node) {
      this.menuNode_ = node;
      return this.menuNode_;
    }
    console.log('Unable to find the Switch Access menu.');
    return this.desktop_;
  }

  /**
   * Clear the focus ring.
   * @private
   */
  clearFocusRing_() {
    this.updateFocusRing_(true);
  }

  /**
   * Determines which menu actions are relevant, given the current node. If
   * there are no node-specific actions, return |null|, to indicate that we
   * should select the current node automatically.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {Array<!SAConstants.MenuAction>}
   * @private
   */
  getActionsForNode_(node) {
    let actions = [];

    let scrollableAncestor = node;
    while (!scrollableAncestor.scrollable && scrollableAncestor.parent)
      scrollableAncestor = scrollableAncestor.parent;

    if (scrollableAncestor.scrollable) {
      if (scrollableAncestor.scrollX > scrollableAncestor.scrollXMin)
        actions.push(SAConstants.MenuAction.SCROLL_LEFT);
      if (scrollableAncestor.scrollX < scrollableAncestor.scrollXMax)
        actions.push(SAConstants.MenuAction.SCROLL_RIGHT);
      if (scrollableAncestor.scrollY > scrollableAncestor.scrollYMin)
        actions.push(SAConstants.MenuAction.SCROLL_UP);
      if (scrollableAncestor.scrollY < scrollableAncestor.scrollYMax)
        actions.push(SAConstants.MenuAction.SCROLL_DOWN);
    }
    const standardActions = /** @type {!Array<!SAConstants.MenuAction>} */ (
        node.standardActions.filter(
            action => Object.values(SAConstants.MenuAction).includes(action)));

    actions = actions.concat(standardActions);

    if (SwitchAccessPredicate.isTextInput(node)) {
      actions.push(SAConstants.MenuAction.KEYBOARD);
      actions.push(SAConstants.MenuAction.DICTATION);

      if (window.switchAccess.textEditingEnabled() &&
          node.state[StateType.FOCUSED]) {
        actions.push(SAConstants.MenuAction.JUMP_TO_BEGINNING_OF_TEXT);
        actions.push(SAConstants.MenuAction.JUMP_TO_END_OF_TEXT);
        actions.push(SAConstants.MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT);
        actions.push(SAConstants.MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT);
        actions.push(SAConstants.MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT);
        actions.push(SAConstants.MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT);
        actions.push(SAConstants.MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT);
        actions.push(SAConstants.MenuAction.MOVE_UP_ONE_LINE_OF_TEXT);
        actions.push(SAConstants.MenuAction.SELECT_START);
        if (this.navigationManager_.selectionStarted()) {
          actions.push(SAConstants.MenuAction.SELECT_END);
        }
      }
    } else if (actions.length > 0) {
      actions.push(SAConstants.MenuAction.SELECT);
    }

    if (actions.length === 0)
      return null;

    actions.push(SAConstants.MenuAction.OPTIONS);
    return actions;
  }

  /**
   * Verify if a specified action is available in the current menu.
   * @param {!SAConstants.MenuAction} action
   * @return {boolean}
   * @private
   */
  isActionAvailable_(action) {
    if (!this.inMenu_)
      return false;
    return this.actions_.includes(action);
  }

  /**
   * Perform a specified action on the Switch Access menu.
   * @param {!SAConstants.MenuAction} action
   */
  performAction(action) {
    if (!window.switchAccess.textEditingEnabled()) {
      this.exit();
    }

    // Whether or not the menu should exit after performing
    // the action.
    let exitAfterAction = true;

    switch (action) {
      case SAConstants.MenuAction.SELECT:
        this.navigationManager_.selectCurrentNode();
        break;
      case SAConstants.MenuAction.KEYBOARD:
        this.navigationManager_.openKeyboard();
        break;
      case SAConstants.MenuAction.DICTATION:
        chrome.accessibilityPrivate.toggleDictation();
        break;
      case SAConstants.MenuAction.OPTIONS:
        window.switchAccess.showOptionsPage();
        break;
      case SAConstants.MenuAction.SCROLL_DOWN:
      case SAConstants.MenuAction.SCROLL_UP:
      case SAConstants.MenuAction.SCROLL_LEFT:
      case SAConstants.MenuAction.SCROLL_RIGHT:
        this.navigationManager_.scroll(action);
        break;
      case SAConstants.MenuAction.JUMP_TO_BEGINNING_OF_TEXT:
        this.navigationManager_.jumpToBeginningOfText();
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.JUMP_TO_END_OF_TEXT:
        this.navigationManager_.jumpToEndOfText();
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT:
        this.navigationManager_.moveBackwardOneCharOfText();
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT:
        this.navigationManager_.moveBackwardOneWordOfText();
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT:
        this.navigationManager_.moveDownOneLineOfText();
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT:
        this.navigationManager_.moveForwardOneCharOfText();
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT:
        this.navigationManager_.moveForwardOneWordOfText();
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.MOVE_UP_ONE_LINE_OF_TEXT:
        this.navigationManager_.moveUpOneLineOfText();
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.SELECT_START:
        this.navigationManager_.setSelectStart();
        this.reloadMenu_(this.navigationManager_.currentNode());
        exitAfterAction = false;
        break;
      case SAConstants.MenuAction.SELECT_END:
        this.navigationManager_.setSelectEnd();
        exitAfterAction = false;
        break;
      default:
        this.navigationManager_.performActionOnCurrentNode(action);
    }

    if (window.switchAccess.textEditingEnabled() && exitAfterAction) {
      this.exit();
    }
  }

  /**
   * Send a message to the menu to update the focus ring around the current
   * node.
   * TODO(anastasi): Revisit focus rings before launch
   * @private
   * @param {boolean=} opt_clear If true, will clear the focus ring.
   */
  updateFocusRing_(opt_clear) {
    if (!this.menuPanel_) {
      console.log('Error: Menu panel has not loaded.');
      return;
    }

    this.calculateCurrentNode();

    if (!this.inMenu_ || !this.node_)
      return;
    let id = this.node_.htmlAttributes.id;

    // If the selection will close the menu, highlight the back button.
    if (id === SAConstants.MENU_ID)
      id = SAConstants.BACK_ID;

    const enable = !opt_clear;
    this.menuPanel_.setFocusRing(id, enable);
  }

  /**
   * Updates the value of |this.node_|.
   *
   * - If it has a value, change nothing.
   * - Otherwise, if menu node has a reasonable value, set |this.node_| to menu
   *   node.
   * - If not, set it to null.
   *
   * Return |this.node_|'s value after the update.
   *
   * @private
   * @return {chrome.automation.AutomationNode}
   */
  calculateCurrentNode() {
    if (this.node_)
      return this.node_;
    this.node_ = this.menuNode();
    if (this.node_ === this.desktop_)
      this.node_ = null;
    return this.node_;
  }
}
