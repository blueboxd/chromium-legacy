// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-add-port-dialog' is a component enabling a
 * user to start forwarding a different port by filling in the appropriate
 * fields and clicking add.
 */
const MIN_VALID_PORT_NUMBER = 1024;   // Minimum 16-bit integer value.
const MAX_VALID_PORT_NUMBER = 65535;  // Maximum 16-bit integer value.

Polymer({
  is: 'settings-crostini-add-port-dialog',

  properties: {
    /**
     * @private {?number}
     */
    inputPortNumber_: {
      type: Number,
      value: null,
    },

    /**
     * @private {string}
     */
    inputPortLabel_: {
      type: String,
      value: '',
    },

    /**
     * @private {number}
     */
    inputProtocolIndex_: {
      type: Number,
      value: 0,  // Default: TCP
    },

    /**
     * @private {boolean}
     */
    invalidPort_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
    this.async(() => {
      this.portNumberInput.focus();
    }, 1);
  },

  resetInputs_: function() {
    this.inputPortLabel_ = '';
    this.inputPortNumber_ = null;
    this.inputProtocolIndex_ = 0;
  },

  /**
   * @returns {string} input for the port number.
   */
  get portNumberInput() {
    return this.$.portNumberInput;
  },

  /**
   * @returns {string} input for the optional port label.
   */
  get portLabelInput() {
    return this.$.portLabelInput;
  },

  /**
   * @param {string} to verify.
   * @returns {boolean} if the input string is a valid port number.
   */
  isValidPortNumber: function(input) {
    const numberRegex = /^[0-9]+$/;
    return input.match(numberRegex) && Number(input) >= MIN_VALID_PORT_NUMBER &&
        Number(input) <= MAX_VALID_PORT_NUMBER;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onSelectProtocol_: function(e) {
    this.inputProtocolIndex_ = e.target.selectedIndex;
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
    this.resetInputs_();
  },

  /** @private */
  onAddTap_: function() {
    if (!this.isValidPortNumber(this.portNumberInput.value)) {
      this.invalidPort_ = true;
      return;
    }
    const portNumber = +this.portNumberInput.value;
    const portLabel = this.portLabelInput.value;
    this.invalidPort_ = false;
    settings.CrostiniBrowserProxyImpl.getInstance()
        .addCrostiniPortForward(
            DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER, portNumber,
            this.inputProtocolIndex_, portLabel)
        .then(result => {
          // TODO(crbug.com/848127): Error handling for result
          this.$.dialog.close();
        });
    this.resetInputs_();
  },
});