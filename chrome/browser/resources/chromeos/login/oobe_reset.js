// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design reset screen.
 */

Polymer({
  is: 'oobe-reset-md',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * State of the screen corresponding to the css style set by
     * oobe_screen_reset.js.
     */
    uiState_: String,

    /**
     * Flag that determines whether help link is shown.
     */
    isGoogleBranded_: Boolean,

    /**
     * Whether to show the TPM firmware update checkbox.
     */
    tpmFirmwareUpdateAvailable_: Boolean,

    /**
     * If the checkbox to request a TPM firmware update is checked.
     */
    tpmFirmwareUpdateChecked_: Boolean,

    /**
     * If the checkbox to request a TPM firmware update is editable.
     */
    tpmFirmwareUpdateEditable_: Boolean,

    /**
     * Reference to OOBE screen object.
     * @type {!OobeTypes.Screen}
     */
    screen: {
      type: Object,
    },
  },

  focus() {
    this.$.resetDialog.focus();
  },

  /** @private */
  isState_(uiState_, state_) {
    return uiState_ == state_;
  },

  /** @private */
  isCancelHidden_(uiState_) {
    return uiState_ == 'revert-promise-view';
  },

  /** @private */
  isHelpLinkHidden_(uiState_, isGoogleBranded_) {
    return !isGoogleBranded_ || (uiState_ == 'revert-promise-view');
  },

  /** @private */
  isTPMFirmwareUpdateHidden_(uiState_, tpmFirmwareUpdateAvailable_) {
    var inProposalView = [
      'powerwash-proposal-view', 'rollback-proposal-view'
    ].includes(uiState_);
    return !(tpmFirmwareUpdateAvailable_ && inProposalView);
  },

  /**
   * On-tap event handler for cancel button.
   *
   * @private
   */
  onCancelTap_() {
    chrome.send('login.ResetScreen.userActed', ['cancel-reset']);
  },

  /**
   * On-tap event handler for restart button.
   *
   * @private
   */
  onRestartTap_() {
    chrome.send('login.ResetScreen.userActed', ['restart-pressed']);
  },

  /**
   * On-tap event handler for powerwash button.
   *
   * @private
   */
  onPowerwashTap_() {
    chrome.send('login.ResetScreen.userActed', ['show-confirmation']);
  },

  /**
   * On-tap event handler for learn more link.
   *
   * @private
   */
  onLearnMoreTap_() {
    chrome.send('login.ResetScreen.userActed', ['learn-more-link']);
  },

  /**
   * Change handler for TPM firmware update checkbox.
   *
   * @private
   */
  onTPMFirmwareUpdateChanged_() {
    this.screen.onTPMFirmwareUpdateChanged_(
        this.$.tpmFirmwareUpdateCheckbox.checked);
  },

  /**
   * On-tap event handler for the TPM firmware update learn more link.
   *
   * @param {!Event} event
   * @private
   */
  onTPMFirmwareUpdateLearnMore_(event) {
    chrome.send(
        'login.ResetScreen.userActed', ['tpm-firmware-update-learn-more-link']);
    event.stopPropagation();
  },

});
