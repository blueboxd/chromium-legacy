// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for the security token PIN dialog shown during
 * sign-in.
 */

Polymer({
  is: 'security-token-pin',

  behaviors: [OobeDialogHostBehavior, I18nBehavior],

  properties: {
    /**
     * Contains the OobeTypes.SecurityTokenPinDialogParameters object. It can be
     * null when our element isn't used.
     *
     * Changing this field resets the dialog state. (Please note that, due to
     * the Polymer's limitation, only assigning a new object is observed;
     * changing just a subproperty won't work.)
     */
    parameters: {
      type: Object,
      observer: 'onParametersChanged_',
    },

    /**
     * Whether the current state is the wait for the processing completion
     * (i.e., the backend is verifying the entered PIN).
     * @private
     */
    processingCompletion_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user has made changes in the input field since the dialog
     * was initialized or reset.
     * @private
     */
    userEdited_: {
      type: Boolean,
      value: false,
    }
  },

  /**
   * Invoked when the "Back" button is clicked.
   * @private
   */
  onBackClicked_: function() {
    this.fire('cancel');
  },

  /**
   * Invoked when the "Next" button is clicked.
   * @private
   */
  onNextClicked_: function() {
    if (this.processingCompletion_) {
      // Race condition: This could happen if Polymer hasn't yet updated the
      // "disabled" state of the "Next" button before the user clicked on it for
      // the second time.
      return;
    }
    this.processingCompletion_ = true;
    this.fire('completed', this.$.pinKeyboard.value);
  },

  /**
   * Observer that is called when the |parameters| property gets changed.
   * @private
   */
  onParametersChanged_: function() {
    // Reset the dialog to the initial state.
    this.$.pinKeyboard.value = '';
    this.processingCompletion_ = false;
    this.userEdited_ = false;
  },

  /**
   * Observer that is called when the user changes the PIN input field.
   * @private
   */
  onPinChange_: function() {
    this.userEdited_ = true;
  },

  /**
   * Returns whether an error message should be displayed to the user.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {boolean}
   * @private
   */
  hasError_: function(parameters, userEdited) {
    if (!parameters)
      return false;
    if (parameters.attemptsLeft != -1)
      return true;
    return parameters.errorLabel !=
        OobeTypes.SecurityTokenPinDialogErrorType.NONE &&
        !userEdited;
  },

  /**
   * Returns the text of the error message to be displayed to the user.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {string}
   * @private
   */
  getErrorText_: function(parameters, userEdited) {
    if (!this.hasError_(parameters, userEdited))
      return '';
    return [
      this.formatErrorLabel_(parameters, userEdited),
      this.formatAttemptsLeft_(parameters)
    ].join(' ');
  },

  /**
   * Returns the formatted PIN error label to be displayed to the user.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {string}
   * @private
   */
  formatErrorLabel_: function(parameters, userEdited) {
    if (!parameters || userEdited)
      return '';
    var errorLabel = '';
    // TODO(crbug.com/964069): Make strings localized.
    switch (parameters.errorLabel) {
      case OobeTypes.SecurityTokenPinDialogErrorType.NONE:
        break;
      case OobeTypes.SecurityTokenPinDialogErrorType.UNKNOWN:
        errorLabel = 'Unknown error.';
        break;
      case OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN:
        errorLabel = 'Invalid PIN.';
        break;
      case OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PUK:
        errorLabel = 'Invalid PUK.';
        break;
      case OobeTypes.SecurityTokenPinDialogErrorType.MAX_ATTEMPTS_EXCEEDED:
        errorLabel = 'Maximum allowed attempts exceeded.';
        break;
      default:
        assertNotReached(`Unexpected enum value: ${parameters.errorLabel}`);
    }
    if (errorLabel && parameters.enableUserInput)
      errorLabel = `${errorLabel} Please try again.`;
    return errorLabel;
  },

  /**
   * Returns the formatted PIN attempts left count to be displayed to the user.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @return {string}
   * @private
   */
  formatAttemptsLeft_: function(parameters) {
    if (!parameters || parameters.attemptsLeft == -1)
      return '';
    // TODO(crbug.com/964069): Make the string localized.
    return `Attempts left: ${parameters.attemptsLeft}`;
  },
});
