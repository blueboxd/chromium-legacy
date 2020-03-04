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
      AutoScanManager.initialize();
      NavigationManager.initialize(desktop);

      Commands.initialize();
      KeyboardRootNode.startWatchingVisibility();
      MenuManager.initialize();
      SwitchAccessPreferences.initialize();
      TextNavigationManager.initialize();
    });
  }

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

  /**
   * Sets up the connection between the menuPanel and menuManager.
   * @param {!PanelInterface} menuPanel
   */
  connectMenuPanel(menuPanel) {
    // Because this may be called before init_(), check if navigationManager_
    // is initialized.

    if (NavigationManager.instance) {
      NavigationManager.instance.connectMenuPanel(menuPanel);
      MenuManager.instance.connectMenuPanel(menuPanel);
    } else {
      window.menuPanel = menuPanel;
    }
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
