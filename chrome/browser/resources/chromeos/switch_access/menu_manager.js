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
     * The root node of the menu panel.
     * @private {chrome.automation.AutomationNode}
     */
    this.menuPanelNode_;

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
     * The node that the menu has been opened for. Null if the menu is not
     * currently open.
     * @private {chrome.automation.AutomationNode}
     */
    this.menuOriginNode_;

    /**
     * Keeps track of when we're in the Switch Access menu.
     * @private {boolean}
     */
    this.inMenu_ = false;

    /**
     * Keeps track of when there's a selection in the current node.
     * @private {boolean}
     */
    this.selectionExists_ = false;

    /**
     * Callback for reloading the menu when the text selection has changed.
     * Bind creates a new function, so this function is saved as a field to
     * add and remove the selection event listener properly.
     * @private {function(chrome.automation.AutomationEvent): undefined}
     */
    this.onSelectionChanged_ = this.reloadMenuForSelectionChange_.bind(this);

    /**
     * Keeps track of when the clipboard is empty.
     * @private {boolean}
     */
    this.clipboardHasData_ = false;

    /**
     * A reference to the Switch Access Menu Panel class.
     * @private {PanelInterface}
     */
    this.menuPanel_;

    /**
     * Callback for highlighting the first available action once a
     * menu has been loaded in the panel. Bind creates a new function, so
     * this function is saved as a field so that the event listener
     * associated with this callback can be removed properly.
     * @private {function(chrome.automation.AutomationEvent): undefined}
     */
    this.onMenuPanelChildrenChanged_ = this.highlightFirstAction_.bind(this);

    /**
     * A stack to keep track of all menus that have been opened before
     * the current menu (so the top of the stack will be the parent
     * menu of the current menu).
     * @private {!Array<SAConstants.MenuId>}
     */
    this.menuStack_ = [];

    this.init_();
  }

  /**
   * Set up clipboardListener for showing/hiding paste button.
   * @private
   */
  init_() {
    if (window.switchAccess.improvedTextInputEnabled()) {
      chrome.clipboard.onClipboardDataChanged.addListener(
          this.updateClipboardHasData.bind(this));
    }
  }

  /**
   * If multiple actions are available for the currently highlighted node,
   * opens the main menu. Otherwise, selects the node by default.
   * @param {!chrome.automation.AutomationNode} navNode the currently
   * highlighted node, for which the menu is to be displayed.
   */
  enter(navNode) {
    if (!this.menuPanel_) {
      console.log('Error: Menu panel has not loaded.');
      return;
    }

    if (!this.openMenu_(navNode, SAConstants.MenuId.MAIN)) {
      // openMenu_ will return false (indicating that the menu was not opened
      // successfully) when there is only one interesting action (selection)
      // specific to this node. In this case, rather than forcing the user to
      // repeatedly disambiguate, we will simply select by default.
      this.navigationManager_.selectCurrentNode();
      return;
    }

    this.inMenu_ = true;
  }

  /**
   * Exits the menu.
   */
  exit() {
    this.closeCurrentMenu_();
    this.inMenu_ = false;

    if (window.switchAccess.improvedTextInputEnabled() &&
        this.menuOriginNode_) {
      this.menuOriginNode_.removeEventListener(
          chrome.automation.EventType.TEXT_SELECTION_CHANGED,
          this.onSelectionChanged_, false /** Don't use capture. */);
    }
    this.menuOriginNode_ = null;

    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        false /** Hide the menu. */, RectHelper.ZERO_RECT, 0);
  }

  /**
   * Opens the menu with given |menuId|. Shows the menu actions that are
   * applicable to the currently highlighted node in the menu panel. If the
   * menu being opened is the same as the current menu open (i.e. the menu is
   * being reloaded), then the action that triggered the reload
   * will be highlighted. Otherwise, the first available action will
   * be highlighted. Returns a boolean of whether or not the menu was
   * successfully opened.
   * @param {!chrome.automation.AutomationNode} navNode The currently
   *     highlighted node, for which the menu is being opened.
   * @param {!SAConstants.MenuId} menuId Indicates the menu being opened.
   * @param {boolean=} isSubmenu Whether or not the menu being opened is a
   *     submenu of the current menu.
   * @return {boolean} Whether or not the menu was successfully opened.
   * @private
   */
  openMenu_(navNode, menuId, isSubmenu = false) {
    // Action currently highlighted in the menu (null if the menu was closed
    // before this function was called).
    const actionNode = this.node_;

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

    if (!shouldReloadMenu) {
      // Wait for the menu to appear in the panel before highlighting the
      // first available action.
      this.menuPanelNode().addEventListener(
          chrome.automation.EventType.CHILDREN_CHANGED,
          this.onMenuPanelChildrenChanged_, false /** Don't use capture. */);
    }

    // Converting to JSON strings to check equality of Array contents.
    if (JSON.stringify(actions) !== JSON.stringify(this.actions_)) {
      // Set new menu actions in the panel.
      this.actions_ = actions;
      this.menuPanel_.setActions(this.actions_, menuId);
    }

    if (!navNode.location) {
      console.log('Unable to show Switch Access menu.');
      return false;
    }
    // Show the menu panel.
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        true, navNode.location, actions.length);

    this.menuOriginNode_ = navNode;
    if (!shouldReloadMenu && window.switchAccess.improvedTextInputEnabled()) {
      this.menuOriginNode_.addEventListener(
          chrome.automation.EventType.TEXT_SELECTION_CHANGED,
          this.onSelectionChanged_, false /** Don't use capture. */);
    }

    if (shouldReloadMenu && actionNode) {
      let buttonId = actionNode.htmlAttributes.id;
      if (actions.includes(buttonId)) {
        // Highlight the same action that was highlighted before the menu was
        // reloaded.
        this.node_ = actionNode;
        this.updateFocusRing_();
      } else {
        while (!actions.includes(buttonId) && buttonId != 'back') {
          this.moveForward();
          buttonId = this.node_.htmlAttributes.id;
        }
      }
    }
    return true;
  }

  /**
   * Closes the current menu and clears the menu panel.
   * @private
   */
  closeCurrentMenu_() {
    this.clearFocusRing_();
    if (this.node_) {
      this.node_ = null;
    }
    this.menuPanel_.clear();
    this.actions_ = [];
    this.menuNode_ = null;
  }

  /**
   * Get the actions applicable for |navNode| from the menu with given
   * |menuId|.
   * @param {!chrome.automation.AutomationNode} navNode The currently selected
   *     node, for which the menu is being opened.
   * @param {SAConstants.MenuId} menuId
   * @return {Array<SAConstants.MenuAction>}
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
   * @return {!Array<SAConstants.MenuAction>}
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
    let firstNode =
        this.menuNode().find({role: chrome.automation.RoleType.BUTTON});

    while (firstNode && !this.isActionAvailable_(firstNode.htmlAttributes.id))
      firstNode = firstNode.nextSibling;

    if (firstNode) {
      this.node_ = firstNode;
      this.updateFocusRing_();
    }

    // The event is fired multiple times when a new menu is opened in the
    // panel, so remove the listener once the callback has been called once.
    // This ensures the first action is not continually highlighted as we
    // navigate through the menu.
    this.menuPanelNode().removeEventListener(
        chrome.automation.EventType.CHILDREN_CHANGED,
        this.onMenuPanelChildrenChanged_, false /** Don't use capture. */);
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
   * Perform the action indicated by the current button.
   * @return {boolean} Whether this function had any effect.
   */
  selectCurrentNode() {
    this.calculateCurrentNode();

    if (!this.inMenu_ || !this.node_) {
      return false;
    }

    if (this.node_.role === RoleType.BUTTON) {
      // A menu action was selected.
      this.node_.doDefault();
    } else {
      // The back button was selected.
      this.selectBackButton();
    }
    return true;
  }

  /**
   * Selects the back button for the menu. If the current menu is a submenu
   * (i.e. not the main menu), then the current menu will be
   * closed and the parent menu that opened the current menu will be re-opened.
   * If the current menu is the main menu, then exit the menu panel entirely
   * and return to traditional navigation.
   */
  selectBackButton() {
    // Id of the menu that opened the current menu (null if the current
    // menu is the main menu and not a submenu).
    const parentMenuId = this.menuStack_.pop();
    if (parentMenuId && this.menuOriginNode_) {
      // Re-open the parent menu.
      this.openMenu_(this.menuOriginNode_, parentMenuId);
    } else {
      this.exit();
    }
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
   * Get the menu panel node. If it's not defined, search for it.
   * @return {!chrome.automation.AutomationNode}
   */
  menuPanelNode() {
    if (this.menuPanelNode_) {
      return this.menuPanelNode_;
    }

    const treeWalker = new AutomationTreeWalker(
        this.desktop_, constants.Dir.FORWARD,
        SwitchAccessPredicate.switchAccessMenuPanelDiscoveryRestrictions());
    const node = treeWalker.next().node;
    if (node) {
      this.menuPanelNode_ = node;
      return this.menuPanelNode_;
    }
    console.log('Unable to find the Switch Access menu panel.');
    return this.desktop_;
  }

  /**
   * Get the menu node. With the current design, the menu panel should
   * always contain at most one menu. When a menu is open in the panel,
   * the menu node is the first child of the menu panel node.
   * @return {!chrome.automation.AutomationNode}
   */
  menuNode() {
    if (this.menuNode_) {
      return this.menuNode_;
    }

    if (this.menuPanelNode() !== this.desktop_) {
      if (this.menuPanelNode_.firstChild) {
        this.menuNode_ = this.menuPanelNode_.firstChild;
        return this.menuNode_;
      }
    }

    return this.desktop_;
  }

  /**
   * Whether or not the menu is currently open.
   * @return {boolean}
   * @public
   */
  inMenu() {
    return this.inMenu_;
  }

  /**
   * TODO(rosalindag): Add functionality to catch when clipboardHasData_ needs
   * to be set to false.
   * Set the clipboardHasData variable to true and reload the menu.
   */
  updateClipboardHasData() {
    this.clipboardHasData_ = true;
    if (this.menuOriginNode_) {
      this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
    }
  }

  /**
   * Clear the focus ring.
   * @private
   */
  clearFocusRing_() {
    this.updateFocusRing_(true);
  }

  /**
   * Returns if there is a selection in the current node.
   * @private
   * @returns {boolean} whether or not there's a selection
   */
  nodeHasSelection_() {
    if (this.menuOriginNode_) {
      if (this.menuOriginNode_.textSelStart !==
          this.menuOriginNode_.textSelEnd) {
        return true;
      } else {
        return false;
      }
    }
    return false;
  }

  /**
   * Check to see if there is a change in the selection in the current node and
   * reload the menu if so.
   * @private
   */
  reloadMenuForSelectionChange_() {
    let newSelectionState = this.nodeHasSelection_();
    if (this.selectionExists_ != newSelectionState) {
      this.selectionExists_ = newSelectionState;
      if (this.menuOriginNode_ &&
          !this.navigationManager_.currentlySelecting()) {
        let currentMenuId = this.menuPanel_.currentMenuId();
        if (currentMenuId) {
          this.openMenu_(this.menuOriginNode_, currentMenuId);
        } else {
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        }
      }
    }
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
  getMainMenuActionsForNode_(node) {
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

      if (window.switchAccess.improvedTextInputEnabled() &&
          node.state[StateType.FOCUSED]) {
        actions.push(SAConstants.MenuAction.MOVE_CURSOR);
        actions.push(SAConstants.MenuAction.SELECT_START);
        if (this.navigationManager_.currentlySelecting()) {
          actions.push(SAConstants.MenuAction.SELECT_END);
        }
        if (this.selectionExists_) {
          actions.push(SAConstants.MenuAction.CUT);
          actions.push(SAConstants.MenuAction.COPY);
        }
        if (this.clipboardHasData_) {
          actions.push(SAConstants.MenuAction.PASTE);
        }
      }
    } else if (actions.length > 0) {
      actions.push(SAConstants.MenuAction.SELECT);
    }

    if (actions.length === 0)
      return null;

    actions.push(SAConstants.MenuAction.SETTINGS);
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
    if (!window.switchAccess.improvedTextInputEnabled()) {
      this.exit();
    }

    switch (action) {
      case SAConstants.MenuAction.SELECT:
        if (window.switchAccess.improvedTextInputEnabled()) {
          // Explicitly call exit for actions where the menu closes after
          // the icon is clicked.
          this.exit();
        }
        this.navigationManager_.selectCurrentNode();
        break;
      case SAConstants.MenuAction.KEYBOARD:
        if (window.switchAccess.improvedTextInputEnabled()) {
          this.exit();
        }
        this.navigationManager_.openKeyboard();
        break;
      case SAConstants.MenuAction.DICTATION:
        if (window.switchAccess.improvedTextInputEnabled()) {
          this.exit();
        }
        chrome.accessibilityPrivate.toggleDictation();
        break;
      case SAConstants.MenuAction.SETTINGS:
        if (window.switchAccess.improvedTextInputEnabled()) {
          this.exit();
        }
        chrome.accessibilityPrivate.openSettingsSubpage(
            'manageAccessibility/switchAccess');
        break;
      case SAConstants.MenuAction.SCROLL_DOWN:
      case SAConstants.MenuAction.SCROLL_UP:
      case SAConstants.MenuAction.SCROLL_LEFT:
      case SAConstants.MenuAction.SCROLL_RIGHT:
        if (window.switchAccess.improvedTextInputEnabled()) {
          this.exit();
        }
        this.navigationManager_.scroll(action);
        break;
      case SAConstants.MenuAction.MOVE_CURSOR:
        if (this.menuOriginNode_) {
          this.openMenu_(
              this.menuOriginNode_, SAConstants.MenuId.TEXT_NAVIGATION,
              true /** Opening a submenu. */);
        }
        break;
      case SAConstants.MenuAction.JUMP_TO_BEGINNING_OF_TEXT:
        this.navigationManager_.jumpToBeginningOfText();
        break;
      case SAConstants.MenuAction.JUMP_TO_END_OF_TEXT:
        this.navigationManager_.jumpToEndOfText();
        break;
      case SAConstants.MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT:
        this.navigationManager_.moveBackwardOneCharOfText();
        break;
      case SAConstants.MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT:
        this.navigationManager_.moveBackwardOneWordOfText();
        break;
      case SAConstants.MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT:
        this.navigationManager_.moveDownOneLineOfText();
        break;
      case SAConstants.MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT:
        this.navigationManager_.moveForwardOneCharOfText();
        break;
      case SAConstants.MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT:
        this.navigationManager_.moveForwardOneWordOfText();
        break;
      case SAConstants.MenuAction.MOVE_UP_ONE_LINE_OF_TEXT:
        this.navigationManager_.moveUpOneLineOfText();
        break;
      case SAConstants.MenuAction.CUT:
        this.navigationManager_.cut();
        break;
      case SAConstants.MenuAction.COPY:
        this.navigationManager_.copy();
        break;
      case SAConstants.MenuAction.PASTE:
        this.navigationManager_.paste();
        break;
      case SAConstants.MenuAction.SELECT_START:
        this.navigationManager_.saveSelectStart();
        if (this.menuOriginNode_) {
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        }
        break;
      case SAConstants.MenuAction.SELECT_END:
        this.navigationManager_.endSelection();
        if (this.menuOriginNode_)
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        break;
      default:
        if (window.switchAccess.improvedTextInputEnabled()) {
          this.exit();
        }
        this.navigationManager_.performActionOnCurrentNode(action);
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
    if (id === this.menuPanel_.currentMenuId()) {
      id = SAConstants.BACK_ID;
    }

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
