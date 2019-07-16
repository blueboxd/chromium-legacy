// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage SwitchAccess and interact with other controllers.
 * @implements {SwitchAccessInterface}
 */
class SwitchAccess {
  constructor() {
    console.log('Switch access is enabled');

    /**
     * User commands.
     * @private {Commands}
     */
    this.commands_ = null;

    /**
     * User preferences.
     * @private {SwitchAccessPreferences}
     */
    this.switchAccessPreferences_ = null;

    /**
     * Handles changes to auto-scan.
     * @private {AutoScanManager}
     */
    this.autoScanManager_ = null;

    /**
     * Handles keyboard input.
     * @private {KeyEventHandler}
     */
    this.keyEventHandler_ = null;

    /**
     * Handles interactions with the accessibility tree, including moving to and
     * selecting nodes.
     * @private {NavigationManager}
     */
    this.navigationManager_ = null;

    /**
     * Callback for testing use only.
     * @private {?function()}
     */
    this.onMoveForwardForTesting_ = null;

    /**
     * Callback that is called once the navigation manager is initialized.
     * Used to setup communications with the menu panel.
     * @private {?function()}
     */
    this.navReadyCallback_ = null;

    /**
     * Feature flag controlling improvement of text input capabilities.
     * @private {boolean}
     */
    this.enableTextEditing_ = false;

    this.init_();
  }

  /**
   * Set up preferences, controllers, and event listeners.
   * @private
   */
  init_() {
    this.commands_ = new Commands(this);
    this.switchAccessPreferences_ = new SwitchAccessPreferences(this);
    this.autoScanManager_ = new AutoScanManager(this);
    this.keyEventHandler_ = new KeyEventHandler(this);

    chrome.automation.getDesktop(function(desktop) {
      this.navigationManager_ = new NavigationManager(desktop);

      if (this.navReadyCallback_)
        this.navReadyCallback_();
    }.bind(this));

    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-switch-access-text', (result) => {
          this.enableTextEditing_ = result;
        });
  }

  /**
   * Open and jump to the Switch Access menu.
   * @override
   */
  enterMenu() {
    if (this.navigationManager_)
      this.navigationManager_.enterMenu();
  }

  /**
   * Move to the next interesting node.
   * @override
   */
  moveForward() {
    if (this.navigationManager_)
      this.navigationManager_.moveForward();
    this.onMoveForwardForTesting_ && this.onMoveForwardForTesting_();
  }

  /**
   * Move to the previous interesting node.
   * @override
   */
  moveBackward() {
    if (this.navigationManager_)
      this.navigationManager_.moveBackward();
  }

  /**
   * Perform the default action on the current node.
   * @override
   */
  selectCurrentNode() {
    if (this.navigationManager_)
      this.navigationManager_.selectCurrentNode();
  }

  /**
   * Open the options page in a new tab.
   * @override
   */
  showOptionsPage() {
    const optionsPage = {url: 'options.html'};
    chrome.tabs.create(optionsPage);
  }

  /**
   * Returns whether or not the feature flag
   * for text editing is enabled.
   * @return {boolean}
   * @public
   */
  textEditingEnabled() {
    return this.enableTextEditing_;
  }

  /**
   * Return a list of the names of all user commands.
   * @override
   * @return {!Array<!SAConstants.Command>}
   */
  getCommands() {
    return Object.values(SAConstants.Command);
  }

  /**
   * Checks if the given string is a valid Switch Access command.
   * @param {string} command
   * @return {boolean}
   */
  hasCommand(command) {
    return Object.values(SAConstants.Command).includes(command);
  }

  /**
   * Forwards the keycodes received from keyPressed events to |callback|.
   * @param {function(number)} callback
   */
  listenForKeycodes(callback) {
    this.keyEventHandler_.listenForKeycodes(callback);
  }

  /**
   * Stops forwarding keycodes.
   */
  stopListeningForKeycodes() {
    this.keyEventHandler_.stopListeningForKeycodes();
  }

  /**
   * Run the function binding for the specified command.
   * @override
   * @param {!SAConstants.Command} command
   */
  runCommand(command) {
    this.commands_.runCommand(command);
  }

  /**
   * Perform actions as the result of actions by the user. Currently, restarts
   * auto-scan if it is enabled.
   * @override
   */
  performedUserAction() {
    this.autoScanManager_.restartIfRunning();
  }

  /**
   * Handle a change in user preferences.
   * @override
   * @param {!Object} changes
   */
  onPreferencesChanged(changes) {
    for (const key of Object.keys(changes)) {
      switch (key) {
        case 'enableAutoScan':
          this.autoScanManager_.setEnabled(changes[key]);
          break;
        case 'autoScanTime':
          this.autoScanManager_.setScanTime(changes[key]);
          break;
        default:
          if (this.hasCommand(key))
            this.keyEventHandler_.updateSwitchAccessKeys();
      }
    }
  }

  /**
   * Set the value of the preference |key| to |value| in chrome.storage.sync.
   * Once the storage is set, the Switch Access preferences/behavior are
   * updated.
   *
   * @override
   * @param {SAConstants.Preference} key
   * @param {boolean|number} value
   */
  setPreference(key, value) {
    this.switchAccessPreferences_.setPreference(key, value);
  }

  /**
   * Get the boolean value for the given key. Will throw a type error if the
   * value associated with |key| is not a boolean, or undefined.
   *
   * @override
   * @param  {SAConstants.Preference} key
   * @return {boolean}
   */
  getBooleanPreference(key) {
    return this.switchAccessPreferences_.getBooleanPreference(key);
  }

  /**
   * Get the number value for the given key. Will throw a type error if the
   * value associated with |key| is not a number, or undefined.
   *
   * @override
   * @param  {SAConstants.Preference} key
   * @return {number}
   */
  getNumberPreference(key) {
    return this.switchAccessPreferences_.getNumberPreference(key);
  }

  /**
   * Get the number value for the given key, or |null| if none exists.
   *
   * @override
   * @param  {SAConstants.Preference} key
   * @return {number|null}
   */
  getNumberPreferenceIfDefined(key) {
    return this.switchAccessPreferences_.getNumberPreferenceIfDefined(key);
  }

  /**
   * Returns true if |keyCode| is already used to run a command from the
   * keyboard.
   *
   * @override
   * @param {number} keyCode
   * @return {boolean}
   */
  keyCodeIsUsed(keyCode) {
    return this.switchAccessPreferences_.keyCodeIsUsed(keyCode);
  }

  /**
   * Sets up the connection between the menuPanel and menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {MenuManager}
   */
  connectMenuPanel(menuPanel) {
    // Because this may be called before init_(), check if navigationManager_
    // is initialized.
    if (this.navigationManager_)
      return this.navigationManager_.connectMenuPanel(menuPanel);

    // If not, set navReadyCallback_ to have the menuPanel try again.
    this.navReadyCallback_ = menuPanel.connectToBackground.bind(menuPanel);
    return null;
  }
}
