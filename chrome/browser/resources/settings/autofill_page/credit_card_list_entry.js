// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'credit-card-list-entry' is a credit card row to be shown in
 * the settings page.
 */

cr.define('settings', function() {
  /** @typedef {chrome.autofillPrivate.CreditCardEntry} */
  /* #export */ let CreditCardEntry;

  Polymer({
    is: 'settings-credit-card-list-entry',

    behaviors: [
      I18nBehavior,
    ],

    properties: {
      /**
       * A saved credit card.
       * @type {!settings.CreditCardEntry}
       */
      creditCard: Object,
    },

    /**
     * Opens the credit card action menu.
     * @private
     */
    onDotsMenuClick_() {
      this.fire('dots-card-menu-click', {
        creditCard: this.creditCard,
        anchorElement: this.$$('#creditCardMenu'),
      });
    },

    /** @private */
    onRemoteEditClick_() {
      this.fire('remote-card-menu-click');
    },

    /**
     * The 3-dot menu should not be shown if the card is entirely remote.
     * @return {boolean}
     * @private
     */
    showDots_() {
      return !!(
          this.creditCard.metadata.isLocal ||
          this.creditCard.metadata.isCached);
    },
  });

  // #cr_define_end
  return {CreditCardEntry: CreditCardEntry};
});
