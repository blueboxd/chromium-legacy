// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage interactions with the accessibility tree, including moving
 * to and selecting nodes.
 */
class NavigationManager {
  /**
   * @param {!chrome.automation.AutomationNode} desktop
   */
  constructor(desktop) {
    /**
     * Handles communication with and navigation within the Switch Access menu.
     * @private {!MenuManager}
     */
    this.menuManager_ = new MenuManager(this, desktop);

    /**
     * Handles the details of showing and hiding the back button, when it
     * appears alone (rather than as part of the menu).
     * @private {!BackButtonManager}
     */
    this.backButtonManager_ = new BackButtonManager(this);

    /**
     * Handles the details of text input, including typing and showing an
     * additional focus ring for context when typing.
     * @private {!TextInputManager}
     */
    this.textInputManager_ = new TextInputManager(this);

    /**
     * Handles text navigation actions.
     * @private {!TextNavigationManager}
     */
    this.textNavigationManager_ = new TextNavigationManager(this);

    /**
     * The desktop node.
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;

    /**
     * The currently highlighted node.
     *
     * @private {!chrome.automation.AutomationNode}
     */
    this.node_ = desktop;

    /**
     * The root of the subtree that the user is navigating through.
     *
     * @private {!chrome.automation.AutomationNode}
     */
    this.scope_ = desktop;

    /**
     * A stack of past scopes. Allows user to traverse back to previous groups
     * after selecting one or more groups. The most recent group is at the end
     * of the array.
     *
     * @private {Array<!chrome.automation.AutomationNode>}
     */
    this.scopeStack_ = [];

    /**
     * Keeps track of if we are currently in a system menu.
     * @private {boolean}
     */
    this.inSystemMenu_ = false;

    /**
     * @private {chrome.accessibilityPrivate.FocusRingInfo}
     */
    this.primaryFocusRing_ = {
      id: SAConstants.Focus.PRIMARY_ID,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.SOLID,
      color: SAConstants.Focus.PRIMARY_COLOR,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    };

    /**
     * @private {chrome.accessibilityPrivate.FocusRingInfo}
     */
    this.scopeFocusRing_ = {
      id: SAConstants.Focus.SCOPE_ID,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: SAConstants.Focus.PRIMARY_COLOR,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    };

    /**
     * The currently highlighted scope object. Tracked for comparison purposes.
     * @private {chrome.automation.AutomationNode}
     */
    this.focusedScope_;

    this.init_();
  }

  /**
   * Get the currently highlighted node.
   * @return {!chrome.automation.AutomationNode} the current node
   */
  currentNode() {
    return this.node_;
  }

  /**
   * Open the Switch Access menu for the currently highlighted node.
   */
  enterMenu() {
    // If the back button is focused, select it.
    if (this.backButtonManager_.select())
      return;

    // If we're currently visiting the Switch Access menu, this command should
    // select the highlighted element.
    if (this.menuManager_.selectCurrentNode())
      return;

    this.menuManager_.enter(this.node_);
  }

  /**
   * Find the previous interesting node and update |this.node_|. If there is no
   * previous node, |this.node_| will be set to the back button.
   */
  moveBackward() {
    if (this.menuManager_.moveBackward())
      return;

    if (this.node_ === this.backButtonManager_.buttonNode()) {
      if (SwitchAccessPredicate.isActionable(this.scope_))
        this.setCurrentNode_(this.scope_);
      else
        this.setCurrentNode_(this.youngestDescendant_(this.scope_));
      return;
    }

    this.startAtValidNode_();

    let treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.BACKWARD,
        SwitchAccessPredicate.restrictions(this.scope_));

    let node = treeWalker.next().node;

    if (node === this.scope_)
      node = this.backButtonManager_.buttonNode();

    if (!node)
      node = this.youngestDescendant_(this.scope_);

    // We can't interact with the desktop node, so skip it.
    if (node === this.desktop_) {
      this.node_ = node;
      this.moveBackward();
      return;
    }

    this.setCurrentNode_(node);
  }

  /**
   * Find the next interesting node, and update |this.node_|. If there is no
   * next node, |this.node_| will be set equal to |this.scope_| to loop again.
   */
  moveForward() {
    if (this.menuManager_.moveForward())
      return;

    this.startAtValidNode_();

    const backButtonNode = this.backButtonManager_.buttonNode();
    if (this.node_ === this.scope_ && backButtonNode) {
      this.setCurrentNode_(backButtonNode);
      return;
    }

    // Replace the back button with the scope to find the following node.
    if (this.node_ === backButtonNode)
      this.node_ = this.scope_;

    let treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.FORWARD,
        SwitchAccessPredicate.restrictions(this.scope_));


    let node = treeWalker.next().node;
    // If treeWalker returns undefined, that means we're at the end of the tree
    // and we should start over.
    if (!node) {
      if (SwitchAccessPredicate.isActionable(this.scope_)) {
        node = this.scope_;
      } else if (backButtonNode) {
        node = backButtonNode;
      } else {
        this.node_ = this.scope_;
        this.moveForward();
        return;
      }
    }

    // We can't interact with the desktop node, so skip it.
    if (node === this.desktop_) {
      this.node_ = node;
      this.moveForward();
      return;
    }

    this.setCurrentNode_(node);
  }

  /**
   * Scrolls the current node in the direction indicated by |scrollAction|.
   * @param {!SAConstants.MenuAction} scrollAction
   */
  scroll(scrollAction) {
    // Find the closest ancestor to the current node that is scrollable.
    let scrollNode = this.node_;
    while (scrollNode && !scrollNode.scrollable)
      scrollNode = scrollNode.parent;
    if (!scrollNode)
      return;

    if (scrollAction === SAConstants.MenuAction.SCROLL_DOWN)
      scrollNode.scrollDown(() => {});
    else if (scrollAction === SAConstants.MenuAction.SCROLL_UP)
      scrollNode.scrollUp(() => {});
    else if (scrollAction === SAConstants.MenuAction.SCROLL_LEFT)
      scrollNode.scrollLeft(() => {});
    else if (scrollAction === SAConstants.MenuAction.SCROLL_RIGHT)
      scrollNode.scrollRight(() => {});
    else if (scrollAction === SAConstants.MenuAction.SCROLL_FORWARD)
      scrollNode.scrollForward(() => {});
    else if (scrollAction === SAConstants.MenuAction.SCROLL_BACKWARD)
      scrollNode.scrollBackward(() => {});
    else
      console.log('Unrecognized scroll action: ', scrollAction);
  }

  /**
   * Moves the text caret to the beginning of the current node.
   * @public
   */
  jumpToBeginningOfText() {
    this.textNavigationManager_.jumpToBeginning();
  }

  /**
   * Moves the text caret to the end of the current node.
   * @public
   */
  jumpToEndOfText() {
    this.textNavigationManager_.jumpToEnd();
  }

  /**
   * Moves the text caret backward one character in the current
   * node.
   * @public
   */
  moveBackwardOneCharOfText() {
    this.textNavigationManager_.moveBackwardOneChar();
  }

  /**
   * Moves the text caret backward one word in the current node.
   * @public
   */
  moveBackwardOneWordOfText() {
    this.textNavigationManager_.moveBackwardOneWord();
  }

  /**
   * Moves the text caret down one line in the current node.
   * @public
   */
  moveDownOneLineOfText() {
    this.textNavigationManager_.moveDownOneLine();
  }

  /**
   * Moves the text caret forward one character in the current
   * node.
   * @public
   */
  moveForwardOneCharOfText() {
    this.textNavigationManager_.moveForwardOneChar();
  }

  /**
   * Moves the text caret forward one word in the current node.
   * @public
   */
  moveForwardOneWordOfText() {
    this.textNavigationManager_.moveForwardOneWord();
  }

  /**
   * Moves the text caret up one line in the current node.
   * @public
   */
  moveUpOneLineOfText() {
    this.textNavigationManager_.moveUpOneLine();
  }

  /**
   * Sets the selectionStart variable based on the selection of the current
   * node.
   * @public
   */
  setSelectStart() {
    this.textNavigationManager_.setSelectStart();
  }

  /**
   * Sets the selectionEnd variable based on the selection of the current node.
   * @public
   */
  setSelectEnd() {
    this.textNavigationManager_.setSelectEnd();
  }

  /**
   * Returns whether or not the first selection index has been set.
   * @return {boolean}
   * @public
   */
  selectionStarted() {
    return this.textNavigationManager_.isSelStartIndexSet();
  }

  /**
   * Perform the default action for the currently highlighted node. If the node
   * is the current scope, go back to the previous scope. If the node is a group
   * other than the current scope, go into that scope. If the node is
   * interesting, perform the default action on it.
   */
  selectCurrentNode() {
    if (this.backButtonManager_.select())
      return;

    if (this.menuManager_.selectCurrentNode())
      return;

    if (!this.node_.role)
      return;

    if (window.switchAccess.textEditingEnabled()) {
      if (SwitchAccessPredicate.isTextInput(this.node_)) {
        this.node_.focus();
        return;
      }
    }

    if (this.textInputManager_.pressKey(this.node_)) {
      return;
    }

    if (this.node_ === this.scope_) {
      this.node_.doDefault();
      return;
    }

    if (SwitchAccessPredicate.isGroup(this.node_, this.scope_)) {
      this.setScope_(this.node_);
      return;
    }

    this.node_.doDefault();
  }

  /**
   * Performs |action| on the current node, if an appropriate action exists.
   * @param {!SAConstants.MenuAction} action
   */
  performActionOnCurrentNode(action) {
    if (Object.values(chrome.automation.ActionType).includes(action)) {
      this.node_.performStandardAction(
          /** @type {chrome.automation.ActionType} */ (action));
    }
  }

  /**
   * @param {chrome.automation.AutomationNode=} opt_newNode Optionally set the
   *     node that will have the primary focus after exiting.
   */
  exitCurrentScope(opt_newNode) {
    if (this.scopeStack_.length === 0)
      return;
    if (this.inSystemMenu_)
      this.closeSystemMenu_();

    this.node_ = opt_newNode || this.scope_;
    // Find a previous scope that is still valid. The stack here always has
    // at least one valid scope (i.e., the desktop node).
    do {
      this.scope_ = this.scopeStack_.pop();
    } while (!this.scope_.role && this.scopeStack_.length > 0);

    this.updateFocusRings_();
  }

  /**
   * Puts focus on the virtual keyboard, if the current node is a text input.
   * TODO(946190): Handle the case where the user has not enabled the onscreen
   *               keyboard.
   */
  openKeyboard() {
    if (!this.textInputManager_.enterKeyboard(this.node_))
      return;

    chrome.accessibilityPrivate.setVirtualKeyboardVisible(
        true /* is_visible */);
    this.setScope_(this.textInputManager_.getKeyboard(this.desktop_));
  }

  /**
   * If exiting the current scope will not move from inside the keyboard to
   * outside the keyboard, this method does nothing.
   * When we are exiting the keyboard, it resets the scope and removes the text
   * input focus ring.
   * @return {boolean} Whether any action was taken.
   */
  leaveKeyboardIfNeeded() {
    const newScope = this.scopeStack_[this.scopeStack_.length - 1];
    if (!this.textInputManager_.inVirtualKeyboard(this.scope_) ||
        this.textInputManager_.inVirtualKeyboard(newScope)) {
      return false;
    }

    this.textInputManager_.returnToTextFocus();
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(
        false /* isVisible */);
    return true;
  }

  /**
   * Sets up the connection between the menuPanel and menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {!MenuManager}
   */
  connectMenuPanel(menuPanel) {
    this.backButtonManager_.init(menuPanel, this.desktop_);
    return this.menuManager_.connectMenuPanel(menuPanel);
  }

  // ----------------------Private Methods---------------------

  /**
   * Create a new scope stack and set the current scope for |node|.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @private
   */
  buildScopeStack_(node) {
    // Create list of |node|'s ancestors, with highest level ancestor at the
    // end.
    let ancestorList = [];
    while (node.parent) {
      ancestorList.push(node.parent);
      node = node.parent;
    }

    // Starting with desktop as the scope, if an ancestor is a group, set it to
    // the new scope and push the old scope onto the scope stack.
    this.scopeStack_ = [];
    this.scope_ = this.desktop_;
    while (ancestorList.length > 0) {
      let ancestor = ancestorList.pop();
      if (ancestor.role === chrome.automation.RoleType.DESKTOP)
        continue;
      if (SwitchAccessPredicate.isGroup(ancestor, this.scope_)) {
        this.scopeStack_.push(this.scope_);
        this.scope_ = ancestor;
      }
    }
  }

  /**
   * Closes a system menu when the back button is pressed.
   */
  closeSystemMenu_() {
    chrome.accessibilityPrivate.sendSyntheticKeyEvent({
      type: chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyCode: 27  // Esc
    });
    chrome.accessibilityPrivate.sendSyntheticKeyEvent({
      type: chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyCode: 27  // Esc
    });
  }

  /**
   * When an interesting element gains focus on the page, move to it. If an
   * element gains focus but is not interesting, move to the next interesting
   * node after it.
   *
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onFocusChange_(event) {
    if (this.node_ === event.target)
      return;

    // Rebuild scope stack and set scope for focused node.
    this.buildScopeStack_(event.target);

    // Move to focused node.
    this.node_ = event.target;

    // In case the node that gained focus is not a subtreeLeaf.
    if (SwitchAccessPredicate.isInteresting(this.node_, this.scope_))
      this.updateFocusRings_();
    else
      this.moveForward();
  }

  /**
   * When a menu is opened, jump focus to the menu.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onMenuStart_(event) {
    this.setScope_(event.target);
    this.inSystemMenu_ = true;
  }

  /**
   * When a node is removed from the page, move to a new valid node.
   *
   * @param {!chrome.automation.TreeChange} treeChange
   * @private
   */
  onNodeRemoved_(treeChange) {
    // TODO(elichtenberg): Only listen to NODE_REMOVED callbacks. Don't need
    // any others.
    if (treeChange.type !== chrome.automation.TreeChangeType.NODE_REMOVED)
      return;

    // TODO(elichtenberg): Currently not getting NODE_REMOVED event when whole
    // tree is deleted. Once fixed, can delete this. Should only need to check
    // if target is current node.
    let removedByRWA =
        treeChange.target.role === chrome.automation.RoleType.ROOT_WEB_AREA &&
        !this.node_.role;

    if (!removedByRWA && treeChange.target !== this.node_)
      return;

    this.clearFocusRings_();

    // Current node not invalid until after treeChange callback, so move to
    // valid node after callback. Delay added to prevent moving to another
    // node about to be made invalid. If already at a valid node (e.g., user
    // moves to it or focus changes to it), won't need to move to a new node.
    window.setTimeout(function() {
      if (!this.node_.role)
        this.moveForward();
    }.bind(this), 100);
  }

  /**
   * @private
   */
  init_() {
    this.desktop_.addEventListener(
        chrome.automation.EventType.FOCUS, this.onFocusChange_.bind(this),
        false);
    this.desktop_.addEventListener(
        chrome.automation.EventType.MENU_START, this.onMenuStart_.bind(this),
        false);

    // TODO(elichtenberg): Use a more specific filter than ALL_TREE_CHANGES.
    chrome.automation.addTreeChangeObserver(
        chrome.automation.TreeChangeObserverFilter.ALL_TREE_CHANGES,
        this.onNodeRemoved_.bind(this));
  }

  /**
   * Set |this.node_| to |node|, and update its appearance onscreen.
   *
   * @param {!chrome.automation.AutomationNode} node
   */
  setCurrentNode_(node) {
    this.node_ = node;
    this.updateFocusRings_();
  }

  /**
   * Set the scope to the provided node.
   * @param {chrome.automation.AutomationNode} node
   */
  setScope_(node) {
    if (!node)
      return;
    this.scopeStack_.push(this.scope_);
    this.scope_ = node;

    // The first node will come immediately after the back button, so we set
    // |this.node_| to the back button and call |moveForward|.
    const backButtonNode = this.backButtonManager_.buttonNode();
    if (backButtonNode)
      this.node_ = backButtonNode;
    else
      this.node_ = this.scope_;
    this.moveForward();
  }

  /**
   * Checks if this.node_ is valid. If so, do nothing.
   *
   * If this.node_ is not valid, set this.node_ to a valid scope. Will check the
   * current scope and past scopes until a valid scope is found. this.node_
   * is set to that valid scope.
   *
   * @private
   */
  startAtValidNode_() {
    if (this.node_.role)
      return;

    // Current node is invalid, but current scope is still valid, so set node
    // to the current scope.
    if (this.scope_.role)
      this.node_ = this.scope_;

    // Current node and current scope are invalid, so set both to a valid scope
    // from the scope stack. The stack here always has at least one valid scope
    // (i.e., the desktop node).
    while (!this.node_.role && this.scopeStack_.length > 0) {
      this.node_ = this.scopeStack_.pop();
      this.scope_ = this.node_;
    }
  }

  /**
   * Set the focus ring for the current node and scope.
   * @private
   */
  updateFocusRings_() {
    const focusRect = this.node_.location;

    // If the scope element has not changed, we want to use the previously
    // calculated rect as the current scope rect.
    let scopeRect = this.scope_.location;
    if (this.scopeFocusRing_.rects.length && this.scope_ === this.focusedScope_)
      scopeRect = this.scopeFocusRing_.rects[0];
    this.focusedScope_ = this.scope_;

    if (this.node_ === this.backButtonManager_.buttonNode()) {
      this.backButtonManager_.show(scopeRect);

      this.primaryFocusRing_.rects = [];
      this.scopeFocusRing_.rects = [scopeRect];

      chrome.accessibilityPrivate.setFocusRings(
          [this.primaryFocusRing_, this.scopeFocusRing_]);

      return;
    }
    this.backButtonManager_.hide();

    // If the current element is not the back button, the scope rect should
    // expand to contain the focus rect.
    scopeRect = RectHelper.expandToFitWithPadding(
        SAConstants.Focus.SCOPE_BUFFER, scopeRect, focusRect);

    this.primaryFocusRing_.rects = [focusRect];
    this.scopeFocusRing_.rects = [scopeRect];

    chrome.accessibilityPrivate.setFocusRings(
        [this.primaryFocusRing_, this.scopeFocusRing_]);
  }

  /**
   * Clears all focus rings.
   * @private
   */
  clearFocusRings_() {
    this.primaryFocusRing_.rects = [];
    this.scopeFocusRing_.rects = [];
    chrome.accessibilityPrivate.setFocusRings(
        [this.primaryFocusRing_, this.scopeFocusRing_]);
  }

  /**
   * Get the youngest descendant of |node|, if it has one within the current
   * scope.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {!chrome.automation.AutomationNode}
   * @private
   */
  youngestDescendant_(node) {
    const leaf = SwitchAccessPredicate.leaf(this.scope_);
    const visit = SwitchAccessPredicate.visit(this.scope_);

    const result = this.youngestDescendantHelper_(node, leaf, visit);
    if (!result)
      return this.scope_;
    return result;
  }

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {function(!chrome.automation.AutomationNode): boolean} leaf
   * @param {function(!chrome.automation.AutomationNode): boolean} visit
   * @return {chrome.automation.AutomationNode}
   * @private
   */
  youngestDescendantHelper_(node, leaf, visit) {
    if (!node)
      return null;

    if (leaf(node))
      return visit(node) ? node : null;

    const reverseChildren = node.children.reverse();
    for (const child of reverseChildren) {
      const youngest = this.youngestDescendantHelper_(child, leaf, visit);
      if (youngest)
        return youngest;
    }

    return visit(node) ? node : null;
  }

  // ----------------------Debugging Methods------------------------

  /**
   * Prints a debug version of the accessibility tree with annotations of
   * various SwitchAccess properties.
   *
   * To use, got to the console for SwitchAccess and run
   *    switchAccess.automationManager_.printDebugSwitchAccessTree()
   *
   * @param {NavigationManager.DisplayMode} opt_displayMode - an optional
   *     parameter that controls which nodes are printed. Default is
   *     INTERESTING_NODE.
   * @return {SwitchAccessDebugNode|undefined}
   */
  printDebugSwitchAccessTree(
      opt_displayMode = NavigationManager.DisplayMode.INTERESTING_NODE) {
    let allNodes = opt_displayMode === NavigationManager.DisplayMode.ALL;
    let debugRoot =
        NavigationManager.switchAccessDebugTree_(this.desktop_, allNodes);
    if (debugRoot)
      NavigationManager.printDebugNode_(debugRoot, 0, opt_displayMode);
    return debugRoot;
  }
  /**
   * creates a tree for debugging the SwitchAccess predicates, rooted at
   * node, based on the Accessibility tree.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @param {boolean} allNodes
   * @return {SwitchAccessDebugNode|undefined}
   * @private
   */
  static switchAccessDebugTree_(node, allNodes) {
    let debugNode = this.createAnnotatedDebugNode_(node, allNodes);
    if (!debugNode)
      return;

    for (let child of node.children) {
      let dChild = this.switchAccessDebugTree_(child, allNodes);
      if (dChild)
        debugNode.children.push(dChild);
    }
    return debugNode;
  }

  /**
   * Creates a debug node from the given automation node, with annotations of
   * various SwitchAccess properties.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @param {boolean} allNodes
   * @return {SwitchAccessDebugNode|undefined}
   * @private
   */
  static createAnnotatedDebugNode_(node, allNodes) {
    if (!allNodes && !SwitchAccessPredicate.isInterestingSubtree(node))
      return;

    let debugNode = {};
    if (node.role)
      debugNode.role = node.role;
    if (node.name)
      debugNode.name = node.name;

    debugNode.isActionable = SwitchAccessPredicate.isActionable(node);
    debugNode.isGroup = SwitchAccessPredicate.isGroup(node, node);
    debugNode.isInterestingSubtree =
        SwitchAccessPredicate.isInterestingSubtree(node);

    debugNode.children = [];
    debugNode.baseNode = node;
    return debugNode;
  }

  /**
   * Prints the debug subtree rooted at |node| in pre-order.
   *
   * @param {SwitchAccessDebugNode} node
   * @param {!number} indent
   * @param {NavigationManager.DisplayMode} displayMode
   * @private
   */
  static printDebugNode_(node, indent, displayMode) {
    if (!node)
      return;

    let result = ' '.repeat(indent);
    if (node.role)
      result += 'role:' + node.role + ' ';
    if (node.name)
      result += 'name:' + node.name + ' ';
    result += 'isActionable? ' + node.isActionable;
    result += ', isGroup? ' + node.isGroup;
    result += ', isInterestingSubtree? ' + node.isInterestingSubtree;

    switch (displayMode) {
      case NavigationManager.DisplayMode.ALL:
        console.log(result);
        break;
      case NavigationManager.DisplayMode.INTERESTING_SUBTREE:
        if (node.isInterestingSubtree)
          console.log(result);
        break;
      case NavigationManager.DisplayMode.INTERESTING_NODE:
      default:
        if (node.isActionable || node.isGroup)
          console.log(result);
        break;
    }

    let children = node.children || [];
    for (let child of children)
      this.printDebugNode_(child, indent + 2, displayMode);
  }
}

/**
 * Display modes for debugging tree.
 *
 * @enum {string}
 * @const
 */
NavigationManager.DisplayMode = {
  ALL: 'all',
  INTERESTING_SUBTREE: 'interestingSubtree',
  INTERESTING_NODE: 'interestingNode'
};

/**
 * @typedef {{role: (string|undefined),
 *            name: (string|undefined),
 *            isActionable: boolean,
 *            isGroup: boolean,
 *            isInterestingSubtree: boolean,
 *            children: Array<SwitchAccessDebugNode>,
 *            baseNode: chrome.automation.AutomationNode}}
 */
let SwitchAccessDebugNode;
