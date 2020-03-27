// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordCheckListItem represents one leaked credential in the
 * list of compromised passwords.
 */

Polymer({
  is: 'password-check-list-item',

  properties: {
    password_: {
      type: String,
      value: ' '.repeat(10),
    },

    /**
     * The password that is being displayed.
     * @type {!PasswordManagerProxy.CompromisedCredential}
     */
    item: Object,
  },

  /**
   * @return {string}
   * @private
   */
  getCompromiseType_() {
    switch (this.item.compromiseType) {
      case chrome.passwordsPrivate.CompromiseType.PHISHED:
        return loadTimeData.getString('phishedPassword');
      case chrome.passwordsPrivate.CompromiseType.LEAKED:
        return loadTimeData.getString('leakedPassword');
      case chrome.passwordsPrivate.CompromiseType.PHISHED_AND_LEAKED:
        return loadTimeData.getString('phishedAndLeakedPassword');
    }

    assertNotReached(
        'Can\'t find a string for type: ' + this.item.compromiseType);
  },

  /**
   * @private
   */
  onChangePasswordClick_() {
    const url = assert(this.item.changePasswordUrl);
    settings.OpenWindowProxyImpl.getInstance().openURL(url);

    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordManagerProxy.PasswordCheckInteraction.CHANGE_PASSWORD);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMoreClick_(event) {
    this.fire('more-actions-click', {moreActionsButton: event.target});
  },
});
