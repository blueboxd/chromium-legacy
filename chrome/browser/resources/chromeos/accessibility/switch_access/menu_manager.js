// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle interactions with the Switch Access menu, including moving
 * through and selecting actions.
 */

class MenuManager {
  /** @private */
  constructor() {
    /**
     * A list of the Menu actions that are currently enabled.
     * @private {!Array<!SAConstants.MenuAction>}
     */
    this.actions_ = [];

    /**
     * The root node of the menu panel.
     * @private {chrome.automation.AutomationNode}
     */
    this.menuPanelNode_;

    /**
     * The root node of the menu.
     * @private {SARootNode}
     */
    this.menuNode_;

    /**
     * The current node of the menu.
     * @private {SAChildNode}
     */
    this.node_;

    /**
     * The node that the menu has been opened for. Null if the menu is not
     * currently open.
     * @private {SAChildNode}
     */
    this.menuOriginNode_;

    /**
     * Keeps track of when we're in the Switch Access menu.
     * @private {boolean}
     */
    this.inMenu_ = false;

    /**
     * A function to be called when the menu exits.
     * @private {?function()}
     */
    this.onExitCallback_ = null;

    /**
     * A reference to the Switch Access Menu Panel class.
     * @private {PanelInterface}
     */
    this.menuPanel_;

    /**
     * Callback for highlighting the first available action once a menu has been
     *     loaded in the panel. This function is created here, rather than below
     *     with the other methods, because we need a consistent function object
     *     to be able to add and remove the listener. If this function were a
     *     method, it would need to have the |this| reference bound to it, and
     *     each call to |.bind()| creates a new function (meaning the listener
     *     could never be removed).
     * Why does this work? While methods use call scoping (they look for
     *     variables in the context of the call site), fat arrow functions,
     *     like below, use lexical scoping (they look for variables in the
     *     context of where the function was declared). So the proper |this|
     *     object is referenced without the need for binding.
     * @private {function()}
     */
    this.onMenuPanelChildrenChanged_ = () => {
      this.buildMenuTree_();
      this.highlightFirstAction_();
    };

    /**
     * A stack to keep track of all menus that have been opened before
     * the current menu (so the top of the stack will be the parent
     * menu of the current menu).
     * @private {!Array<SAConstants.MenuId>}
     */
    this.menuStack_ = [];

    if (window.menuPanel) {
      this.connectMenuPanel(window.menuPanel);
    }
  }

  static initialize() {
    MenuManager.instance = new MenuManager();
  }

  // ================= Static Methods ==================

  /**
   * If multiple actions are available for the currently highlighted node,
   * opens the main menu. Otherwise, selects the node by default.
   * @param {!SAChildNode} navNode the currently highlighted node, for which the
   *     menu is to be displayed.
   * @return {boolean} True if the menu opened or an action was selected, false
   *     otherwise.
   */
  static enter(navNode) {
    const manager = MenuManager.instance;
    if (!manager) {
      return false;
    }
    if (!manager.menuPanel_) {
      console.log('Error: Menu panel has not loaded.');
      return false;
    }

    // If the menu is already open, select the highlighted element.
    if (MenuManager.selectCurrentNode()) {
      return true;
    }

    if (!manager.openMenu_(navNode, SAConstants.MenuId.MAIN)) {
      // openMenu_ will return false (indicating that the menu was not opened
      // successfully) when there is only one interesting action (selection)
      // specific to manager node. In manager case, rather than forcing the user
      // to repeatedly disambiguate, we will simply select by default.
      return false;
    }

    manager.inMenu_ = true;
    return true;
  }

  /** Exits the menu. */
  static exit() {
    if (MenuManager.instance) {
      MenuManager.instance.exit_();
    }
  }

  /**
   * Move to the next available action in the menu. If this is no next action,
   * focus the whole menu to loop again.
   * @return {boolean} Whether this function had any effect.
   */
  static moveForward() {
    const manager = MenuManager.instance;
    if (!manager || !manager.inMenu_ || !manager.node_) {
      return false;
    }

    manager.clearFocusRing_();
    manager.node_ = manager.node_.next;
    manager.updateFocusRing_();
    return true;
  }

  /**
   * Move to the previous available action in the menu. If we're at the
   * beginning of the list, start again at the end.
   * @return {boolean} Whether this function had any effect.
   */
  static moveBackward() {
    const manager = MenuManager.instance;
    if (!manager || !manager.inMenu_ || !manager.node_) {
      return false;
    }

    manager.clearFocusRing_();
    manager.node_ = manager.node_.previous;
    manager.updateFocusRing_();
    return true;
  }

  /** Reloads the menu, if it has changed. */
  static reloadMenuIfNeeded() {
    const manager = MenuManager.instance;
    if (manager && manager.menuOriginNode_) {
      manager.openMenu_(manager.menuOriginNode_, SAConstants.MenuId.MAIN);
    }
  }

  /**
   * Perform the action indicated by the current button.
   * @return {boolean} Whether this function had any effect.
   */
  static selectCurrentNode() {
    const manager = MenuManager.instance;
    if (!manager || !manager.inMenu_ || !manager.node_) {
      return false;
    }

    if (manager.node_ instanceof BackButtonNode) {
      // The back button was selected.
      manager.selectBackButton_();
    } else {
      // A menu action was selected.
      manager.node_.performAction(SAConstants.MenuAction.SELECT);
    }
    return true;
  }

  /**
   * This method allows the FocusRingManager to request a change to the back
   *      button focus ring, without overriding navigation through the menu.
   * @param {boolean} should_focus If false, indicates the focus ring aroun
   *      the back button should be cleared.
   */
  static requestBackButtonFocusChange(should_focus) {
    // Ignore attempts from outside the class to set focus when the menu is
    // open.
    if (!MenuManager.instance || MenuManager.instance.inMenu_) {
      return;
    }
    MenuManager.instance.updateFocusRing_(should_focus);
  }

  // ================= Instance Methods ==================

  /**
   * Builds the tree for the current menu.
   * @private
   */
  buildMenuTree_() {
    // menu_panel.html controls the contents of the menu panel, and we are
    // guaranteed that the menu will be the first child.
    if (this.menuPanelNode_ && this.menuPanelNode_.firstChild) {
      this.menuNode_ =
          RootNodeWrapper.buildTree(this.menuPanelNode_.firstChild);
    }
  }

  /**
   * Clear the focus ring.
   * @private
   */
  clearFocusRing_() {
    this.updateFocusRing_(false);
  }

  /**
   * Closes the current menu and clears the menu panel.
   * @private
   */
  closeCurrentMenu_() {
    this.clearFocusRing_();
    this.menuPanel_.clear();
    this.actions_ = [];
    this.node_ = null;
    this.menuNode_ = null;
  }

  /**
   * Sets up the connection between the menuPanel and the menuManager.
   * @param {!PanelInterface} menuPanel
   */
  connectMenuPanel(menuPanel) {
    menuPanel.menuManager = this;
    this.menuPanel_ = menuPanel;
    this.findMenuPanelNode_();
  }

  /**
   * Exits the menu.
   * @private
   */
  exit_() {
    if (!this.inMenu_) {
      return;
    }

    this.closeCurrentMenu_();
    this.inMenu_ = false;

    if (this.onExitCallback_) {
      this.onExitCallback_();
      this.onExitCallback_ = null;
    }
    this.menuOriginNode_ = null;

    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        false /** should_show */, RectHelper.ZERO_RECT, 0);
  }

  /**
   * Searches for the menu panel node.
   * @private
   */
  findMenuPanelNode_() {
    const treeWalker = new AutomationTreeWalker(
        NavigationManager.desktopNode, constants.Dir.FORWARD,
        SwitchAccessPredicate.switchAccessMenuPanelDiscoveryRestrictions());
    const node = treeWalker.next().node;
    if (!node) {
      setTimeout(this.findMenuPanelNode_.bind(this), 500);
      return;
    }
    this.menuPanelNode_ = node;
    this.buildMenuTree_();
  }

  /**
   * Determines which menu actions are relevant, given the current node. If
   * there are no node-specific actions, return |null|, to indicate that we
   * should select the current node automatically.
   *
   * @param {!SAChildNode} node
   * @return {Array<!SAConstants.MenuAction>}
   * @private
   */
  getMainMenuActionsForNode_(node) {
    const actions = node.actions;

    // If there is at most one available action, perform it by default.
    if (actions.length <= 1) {
      return null;
    }

    // Add global actions.
    actions.push(SAConstants.MenuAction.SETTINGS);
    return actions;
  }


  /**
   * Get the actions applicable for |navNode| from the menu with given
   * |menuId|.
   * @param {!SAChildNode} navNode The currently selected node, for which the
   *     menu is being opened.
   * @param {SAConstants.MenuId} menuId
   * @return {Array<!SAConstants.MenuAction>}
   * @private
   */
  getMenuActions_(navNode, menuId) {
    switch (menuId) {
      case SAConstants.MenuId.MAIN:
        return this.getMainMenuActionsForNode_(navNode);
      case SAConstants.MenuId.TEXT_NAVIGATION:
        return this.getTextNavigationActions_();
      default:
        return this.getMainMenuActionsForNode_(navNode);
    }
  }

  /**
   * Get the actions in the text navigation submenu.
   * @return {Array<!SAConstants.MenuAction>}
   * @private
   */
  getTextNavigationActions_() {
    return [
      SAConstants.MenuAction.JUMP_TO_BEGINNING_OF_TEXT,
      SAConstants.MenuAction.JUMP_TO_END_OF_TEXT,
      SAConstants.MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT,
      SAConstants.MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT,
      SAConstants.MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT,
      SAConstants.MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT,
      SAConstants.MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT,
      SAConstants.MenuAction.MOVE_UP_ONE_LINE_OF_TEXT
    ];
  }

  /**
   * Highlights the first available action in the menu.
   * @private
   */
  highlightFirstAction_() {
    if (!this.menuNode_) {
      return;
    }
    this.node_ = this.menuNode_.firstChild;
    this.updateFocusRing_();

    // The event is fired multiple times when a new menu is opened in the
    // panel, so remove the listener once the callback has been called once.
    // This ensures the first action is not continually highlighted as we
    // navigate through the menu.
    this.menuPanelNode_.removeEventListener(
        chrome.automation.EventType.CHILDREN_CHANGED,
        this.onMenuPanelChildrenChanged_, false /** Don't use capture. */);
  }

  /**
   * Returns if there is a selection in the current node.
   * @return {boolean} whether or not there's a selection
   * @private
   */
  nodeHasSelection_() {
    const node = this.menuOriginNode_.automationNode;

    if (node && node.textSelStart !== node.textSelEnd) {
      return true;
    }
    return false;
  }

  /**
   * Opens the menu with given |menuId|. Shows the menu actions that are
   * applicable to the currently highlighted node in the menu panel. If the
   * menu being opened is the same as the current menu open (i.e. the menu is
   * being reloaded), then the action that triggered the reload
   * will be highlighted. Otherwise, the first available action will
   * be highlighted. Returns a boolean of whether or not the menu was
   * successfully opened.
   * @param {!SAChildNode} navNode The currently highlighted node, for which the
   *     menu is being opened.
   * @param {!SAConstants.MenuId} menuId Indicates the menu being opened.
   * @param {boolean=} isSubmenu Whether or not the menu being opened is a
   *     submenu of the current menu.
   * @return {boolean} Whether or not the menu was successfully opened.
   * @private
   */
  openMenu_(navNode, menuId, isSubmenu = false) {
    // Action currently highlighted in the menu (null if the menu was closed
    // before this function was called).
    let actionNode = null;
    if (this.node_) {
      actionNode = this.node_.automationNode;
    }

    const currentMenuId = this.menuPanel_.currentMenuId();
    const shouldReloadMenu = (currentMenuId === menuId);

    if (!shouldReloadMenu) {
      // Close the current menu before opening a new one.
      this.closeCurrentMenu_();

      if (currentMenuId && isSubmenu) {
        // Opening a submenu, so push the parent menu onto the stack.
        this.menuStack_.push(currentMenuId);
      }
    }

    const actions = this.getMenuActions_(navNode, menuId);

    if (!actions) {
      return false;
    }

    // Converting to JSON strings to check equality of Array contents.
    if (JSON.stringify(actions) !== JSON.stringify(this.actions_)) {
      // Set new menu actions in the panel.
      this.actions_ = actions;
      this.menuPanel_.setActions(this.actions_, menuId);
    }

    const loc = navNode.location;
    if (!loc) {
      console.log('Unable to show Switch Access menu.');
      return false;
    }
    // Show the menu panel.
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        true, loc, actions.length);

    this.menuOriginNode_ = navNode;

    const autoNode = this.menuOriginNode_.automationNode;
    if (autoNode && !shouldReloadMenu &&
        SwitchAccess.instance.improvedTextInputEnabled()) {
      const callback = this.reloadMenuForSelectionChange_.bind(this);

      autoNode.addEventListener(
          chrome.automation.EventType.TEXT_SELECTION_CHANGED, callback,
          false /** use_capture */);
      this.onExitCallback_ = autoNode.removeEventListener.bind(
          autoNode, chrome.automation.EventType.TEXT_SELECTION_CHANGED,
          callback, false /** use_capture */);
    }

    if (shouldReloadMenu) {
      this.buildMenuTree_();
      const buttonId = actionNode ? actionNode.htmlAttributes.id : '';
      if (actions.includes(buttonId)) {
        // Highlight the same action that was highlighted before the menu was
        // reloaded.
        this.updateFocusRing_();
      } else {
        this.highlightFirstAction_();
      }
    } else {
      // Wait for the menu to appear in the panel before highlighting the
      // first available action.
      this.menuPanelNode_.addEventListener(
          chrome.automation.EventType.CHILDREN_CHANGED,
          this.onMenuPanelChildrenChanged_, false /** use_capture */);
    }

    return true;
  }

  /**
   * Perform a specified action on the Switch Access menu.
   * @param {!SAConstants.MenuAction} action
   */
  performAction(action) {
    SwitchAccessMetrics.recordMenuAction(action);
    // Handle global actions.
    if (action === SAConstants.MenuAction.SETTINGS) {
      chrome.accessibilityPrivate.openSettingsSubpage(
          'manageAccessibility/switchAccess');
      this.exit_();
      return;
    }

    // Handle text editing actions.
    // TODO(anastasi): Move these actions into the nodes themselves.
    switch (action) {
      case SAConstants.MenuAction.MOVE_CURSOR:
        if (this.menuOriginNode_) {
          this.openMenu_(
              this.menuOriginNode_, SAConstants.MenuId.TEXT_NAVIGATION,
              true /** Opening a submenu. */);
        }
        return;
      case SAConstants.MenuAction.JUMP_TO_BEGINNING_OF_TEXT:
        TextNavigationManager.jumpToBeginning();
        return;
      case SAConstants.MenuAction.JUMP_TO_END_OF_TEXT:
        TextNavigationManager.jumpToEnd();
        return;
      case SAConstants.MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT:
        TextNavigationManager.moveBackwardOneChar();
        return;
      case SAConstants.MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT:
        TextNavigationManager.moveBackwardOneWord();
        return;
      case SAConstants.MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT:
        TextNavigationManager.moveDownOneLine();
        return;
      case SAConstants.MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT:
        TextNavigationManager.moveForwardOneChar();
        return;
      case SAConstants.MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT:
        TextNavigationManager.moveForwardOneWord();
        return;
      case SAConstants.MenuAction.MOVE_UP_ONE_LINE_OF_TEXT:
        TextNavigationManager.moveUpOneLine();
        return;
      case SAConstants.MenuAction.SELECT_START:
        TextNavigationManager.saveSelectStart();
        if (this.menuOriginNode_) {
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        }
        return;
      case SAConstants.MenuAction.SELECT_END:
        TextNavigationManager.resetCurrentlySelecting();
        if (this.menuOriginNode_) {
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        }
        return;
    }

    // Otherwise, ask the node to perform the action itself.
    if (this.menuOriginNode_.performAction(action) ===
        SAConstants.ActionResponse.CLOSE_MENU) {
      this.exit_();
    }
  }

  /**
   * Check to see if there is a change in the selection in the current node and
   * reload the menu if so.
   * @private
   */
  reloadMenuForSelectionChange_() {
    const newSelectionState = this.nodeHasSelection_();
    if (TextNavigationManager.selectionExists != newSelectionState) {
      TextNavigationManager.selectionExists = newSelectionState;
      if (this.menuOriginNode_ && !TextNavigationManager.currentlySelecting()) {
        const currentMenuId = this.menuPanel_.currentMenuId();
        if (currentMenuId) {
          this.openMenu_(this.menuOriginNode_, currentMenuId);
        } else {
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        }
      }
    }
  }

  /**
   * Selects the back button for the menu. If the current menu is a submenu
   * (i.e. not the main menu), then the current menu will be
   * closed and the parent menu that opened the current menu will be re-opened.
   * If the current menu is the main menu, then exit the menu panel entirely
   * and return to traditional navigation.
   * @private
   */
  selectBackButton_() {
    // Id of the menu that opened the current menu (null if the current
    // menu is the main menu and not a submenu).
    const parentMenuId = this.menuStack_.pop();
    if (parentMenuId && this.menuOriginNode_) {
      // Re-open the parent menu.
      this.openMenu_(this.menuOriginNode_, parentMenuId);
    } else {
      this.exit_();
    }
  }

  /**
   * Send a message to the menu to update the focus ring around the current
   * node.
   * TODO(anastasi): Use real focus rings in the menu
   * @param {boolean} should_set If true, will set the focus ring.
   *      If false, will clear the focus ring.
   * @private
   */
  updateFocusRing_(should_set = true) {
    if (!this.menuPanel_) {
      console.log('Error: Menu panel has not loaded.');
      return;
    }

    let id = this.inMenu_ && this.node_ ?
        this.node_.automationNode.htmlAttributes.id :
        SAConstants.BACK_ID;

    // If the selection will close the menu, highlight the back button.
    if (id === this.menuPanel_.currentMenuId()) {
      id = SAConstants.BACK_ID;
    }

    this.menuPanel_.setFocusRing(id, should_set);
  }
}
