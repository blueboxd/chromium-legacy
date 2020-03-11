// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'payments-list' is a list of saved payment methods (credit
 * cards etc.) to be shown in the settings page.
 */

Polymer({
  is: 'settings-payments-list',

  properties: {
    /**
     * An array of all saved credit cards.
     * @type {!Array<!settings.CreditCardEntry>}
     */
    creditCards: Array,

    /**
     * An array of all saved UPI Virtual Payment Addresses.
     * @type {!Array<!string>}
     */
    upiIds: Array,

    /**
     * True if displaying UPI IDs in settings is enabled.
     */
    enableUpiIds_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showUpiIdSettings');
      },
    },
  },

  /**
   * Returns true if the list exists and has items.
   * @param {Array<?>} list
   * @return {boolean}
   * @private
   */
  hasSome_(list) {
    return !!(list && list.length);
  },

  /**
   * Returns true iff there are credit cards to be shown.
   * @return {boolean}
   * @private
   */
  showCreditCards_() {
    return this.hasSome_(this.creditCards);
  },

  /**
   * Returns true iff both credit cards and UPI IDs will be shown.
   * @return {boolean}
   * @private
   */
  showCreditCardUpiSeparator_() {
    return this.showCreditCards_() && this.showUpiIds_();
  },

  /**
   * Returns true iff there UPI IDs and they should be shown.
   * @return {boolean}
   * @private
   */
  showUpiIds_() {
    return this.enableUpiIds_ && this.hasSome_(this.upiIds);
  },

  /**
   * Returns true iff any payment methods will be shown.
   * @return {boolean}
   * @private
   */
  showAnyPaymentMethods_() {
    return this.showCreditCards_() || this.showUpiIds_();
  },
});
