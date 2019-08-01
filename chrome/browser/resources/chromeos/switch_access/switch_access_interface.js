// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface for controllers to interact with main SwitchAccess class.
 * @interface
 */
class SwitchAccessInterface {
  /**
   * Open and jump to the Switch Access menu.
   */
  enterMenu() {}

  /**
   * Move to the next interesting node.
   */
  moveForward() {}

  /**
   * Move to the previous interesting node.
   */
  moveBackward() {}

  /**
   * Perform the default action on the current node.
   */
  selectCurrentNode() {}

  /**
   * Perform actions as the result of actions by the user. Currently, restarts
   * auto-scan if it is enabled.
   */
  performedUserAction() {}

  /**
   * Handle a change in user preferences.
   * @param {!Object} changes
   */
  onPreferencesChanged(changes) {}

  /**
   * Set the value of the preference |key| to |value| in chrome.storage.sync.
   * The behavior is not updated until the storage update is complete.
   *
   * @param {SAConstants.Preference} key
   * @param {boolean|number} value
   */
  setPreference(key, value) {}

  /**
   * Get the boolean value for the given key. Will throw a type error if the
   * value associated with |key| is not a boolean, or undefined.
   *
   * @param  {SAConstants.Preference} key
   * @return {boolean}
   */
  getBooleanPreference(key) {}

  /**
   * Get the number value for the given key. Will throw a type error if the
   * value associated with |key| is not a number, or undefined.
   *
   * @param  {SAConstants.Preference} key
   * @return {number}
   */
  getNumberPreference(key) {}

  /**
   * Get the number value for the given key, or |null| if none exists.
   *
   * @param  {SAConstants.Preference} key
   * @return {number|null}
   */
  getNumberPreferenceIfDefined(key) {}

  /**
   * Sets up the connection between the menuPanel and the menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {MenuManager}
   */
  connectMenuPanel(menuPanel) {}
}
