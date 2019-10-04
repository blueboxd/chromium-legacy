// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle interactions with the Switch Access back button.
 */

class BackButtonManager {
  /**
   * @param {!NavigationManager} navigationManager
   */
  constructor(navigationManager) {
    /**
     * Keeps track of when the back button is open and appears alone
     * (rather than as part of the menu).
     * @private {boolean}
     */
    this.backButtonOpen_ = false;

    /** @private {!NavigationManager} */
    this.navigationManager_ = navigationManager;

    /** @private {PanelInterface} */
    this.menuPanel_;

    /** @private {chrome.automation.AutomationNode} */
    this.backButtonNode_;
  }

  /**
   * Shows the menu as just a back button in the upper right corner of the
   * node.
   * @param {chrome.accessibilityPrivate.ScreenRect=} nodeLocation
   */
  show(nodeLocation) {
    if (!nodeLocation)
      return;
    this.backButtonOpen_ = true;
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        true, nodeLocation, 0 /* num_actions */);
    this.menuPanel_.setFocusRing(SAConstants.BACK_ID, true);
  }

  /**
   * Resets the effects of show().
   */
  hide() {
    this.backButtonOpen_ = false;
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        false, RectHelper.ZERO_RECT, 0);
    this.menuPanel_.setFocusRing(SAConstants.BACK_ID, false);
  }

  /**
   * Selects the back button, hiding the button and exiting the current scope.
   * @return {boolean} Whether or not the back button was successfully selected.
   */
  select() {
    if (this.navigationManager_.selectBackButtonInMenu()) {
      return true;
    }

    if (!this.backButtonOpen_) {
      return false;
    }

    if (this.navigationManager_.leaveKeyboardIfNeeded()) {
      return true;
    }

    this.navigationManager_.exitCurrentScope();
    return true;
  }

  /**
   * Returns the back button node, if we have found it.
   * @return {chrome.automation.AutomationNode}
   */
  backButtonNode() {
    return this.backButtonNode_;
  }

  /**
   * Finds the back button node.
   * @param {!chrome.automation.AutomationNode} desktop
   * @private
   */
  findBackButtonNode_(desktop) {
    this.backButtonNode_ =
        new AutomationTreeWalker(
            desktop, constants.Dir.FORWARD,
            {visit: (node) => node.htmlAttributes.id === SAConstants.BACK_ID})
            .next()
            .node;
    // TODO(anastasi): Determine appropriate event and listen for it, rather
    // than setting a timeout.
    if (!this.backButtonNode_) {
      setTimeout(this.findBackButtonNode_.bind(this, desktop), 500);
    }
  }

  /**
   * Sets the reference to the menu panel, sets up a click handler for the
   * back button, and finds the back button node.
   * @param {!PanelInterface} menuPanel
   * @param {!chrome.automation.AutomationNode} desktop
   */
  init(menuPanel, desktop) {
    this.menuPanel_ = menuPanel;

    // Ensures that the back button behaves the same way when clicked
    // as when selected.
    const backButtonElement = this.menuPanel_.backButtonElement();
    backButtonElement.addEventListener('click', this.select.bind(this));

    this.findBackButtonNode_(desktop);
  }
}
