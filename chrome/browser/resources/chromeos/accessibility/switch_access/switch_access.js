// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The top-level class for the Switch Access accessibility feature. Handles
 * initialization and small matters that don't fit anywhere else in the
 * codebase.
 */
class SwitchAccess {
  static initialize() {
    window.switchAccess = new SwitchAccess();

    chrome.automation.getDesktop((desktop) => {
      // These two must be initialized before the others.
      AutoScanManager.initialize();
      NavigationManager.initialize(desktop);

      Commands.initialize();
      MenuManager.initialize();
      SwitchAccessPreferences.initialize();
      TextNavigationManager.initialize();

      KeyboardRootNode.startWatchingVisibility();
    });
  }

  // TODO(anastasi): Remove once new menu is being used.
  static get instance() {
    return window.switchAccess;
  }

  /** @private */
  constructor() {
    /**
     * Feature flag controlling improvement of text input capabilities.
     * @private {boolean}
     */
    this.enableImprovedTextInput_ = false;

    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-switch-access-text', (result) => {
          this.enableImprovedTextInput_ = result;
        });
  }

  /**
   * Returns whether or not the feature flag
   * for improved text input is enabled.
   * @return {boolean}
   */
  improvedTextInputEnabled() {
    return this.enableImprovedTextInput_;
  }

  /** TODO(anastasi): Remove this once menu migration is complete. */
  connectMenuPanel() {}

  /**
   * Helper function to robustly find a node fitting a given predicate, even if
   * that node has not yet been created.
   * Used to find the menu and back button.
   * @param {!function(!AutomationNode): boolean} predicate
   * @param {!function(!AutomationNode): void} foundCallback
   */
  static findNodeMatchingPredicate(predicate, foundCallback) {
    const desktop = NavigationManager.desktopNode;
    // First, check if the node is currently in the tree.
    const treeWalker = new AutomationTreeWalker(
        desktop, constants.Dir.FORWARD, {visit: predicate});
    treeWalker.next();
    if (treeWalker.node) {
      foundCallback(treeWalker.node);
      return;
    }
    // If it's not currently in the tree, listen for changes to the desktop
    // tree.
    const onDesktopChildrenChanged = (event) => {
      if (predicate(event.target)) {
        // If the event target is the node we're looking for, we've found it.
        desktop.removeEventListener(
            chrome.automation.EventType.CHILDREN_CHANGED,
            onDesktopChildrenChanged, false);
        foundCallback(event.target);
      } else if (event.target.children.length > 0) {
        // Otherwise, see if one of its children is the node we're looking for.
        const treeWalker = new AutomationTreeWalker(
            event.target, constants.Dir.FORWARD,
            {visit: predicate, root: (node) => node == event.target});
        treeWalker.next();
        if (treeWalker.node) {
          desktop.removeEventListener(
              chrome.automation.EventType.CHILDREN_CHANGED,
              onDesktopChildrenChanged, false);
          foundCallback(treeWalker.node);
        }
      }
    };

    desktop.addEventListener(
        chrome.automation.EventType.CHILDREN_CHANGED, onDesktopChildrenChanged,
        false);
  }

  /*
   * Creates and records the specified error.
   * @param {SAConstants.ErrorType} errorType
   * @param {string} errorString
   * @return {!Error}
   */
  static error(errorType, errorString) {
    const errorTypeCountForUMA = Object.keys(SAConstants.ErrorType).length;
    chrome.metricsPrivate.recordEnumerationValue(
        'Accessibility.CrosSwitchAccess.Error', errorType,
        errorTypeCountForUMA);
    return new Error(errorString);
  }
}
