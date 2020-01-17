// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'update-required-card',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Is device connected to network?
     */
    isConnected: {type: Boolean, value: false},

    updateProgressUnavailable: {type: Boolean, value: true},

    updateProgressValue: {type: Number, value: 0},

    updateProgressMessage: {type: String, value: ''},

    estimatedTimeLeftVisible: {type: Boolean, value: false},

    /**
     * Message "3 minutes left".
     */
    estimatedTimeLeft: {
      type: String,
    },

    ui_state: {type: String},
  },

  /** Called after resources are updated. */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * @private
   */
  onSelectNetworkClicked_() {
    chrome.send('login.UpdateRequiredScreen.userActed', ['select-network']);
  },

  /**
   * @private
   */
  onUpdateClicked_() {
    chrome.send('login.UpdateRequiredScreen.userActed', ['update']);
  },

  /**
   * @private
   */
  onFinishClicked_() {
    chrome.send('login.UpdateRequiredScreen.userActed', ['finish']);
  },

  /**
   * @private
   */
  onCellularPermissionRejected_() {
    chrome.send(
        'login.UpdateRequiredScreen.userActed', ['update-reject-cellular']);
  },

  /**
   * @private
   */
  onCellularPermissionAccepted_() {
    chrome.send(
        'login.UpdateRequiredScreen.userActed', ['update-accept-cellular']);
  },

  /**
   * @private
   */
  showOn_(ui_state) {
    // Negate the value as it used as |hidden| attribute's value.
    return !(Array.prototype.slice.call(arguments, 1).includes(ui_state));
  },
});
