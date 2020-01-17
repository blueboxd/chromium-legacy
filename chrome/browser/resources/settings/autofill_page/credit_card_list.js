// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'credit-card-list' is a a list of credit cards to be shown in
 * the settings page.
 */

Polymer({
  is: 'settings-credit-card-list',

  properties: {
    /**
     * An array of all saved credit cards.
     * @type {!Array<!settings.PaymentsManager.CreditCardEntry>}
     */
    creditCards: Array,
  },

  /**
   * Returns true if the list exists and has items.
   * @param {Array<Object>} list
   * @return {boolean}
   * @private
   */
  hasSome_(list) {
    return !!(list && list.length);
  },
});
