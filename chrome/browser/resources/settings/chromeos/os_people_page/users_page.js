// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-page' is the settings page for managing user accounts on
 * the device.
 */
Polymer({
  is: 'settings-users-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    isOwner_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    isWhitelistManaged_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isChild_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isSupervised');
      },
    },
  },

  /** @override */
  created() {
    chrome.usersPrivate.getCurrentUser(user => {
      this.isOwner_ = user.isOwner;
    });

    chrome.usersPrivate.isWhitelistManaged(isWhitelistManaged => {
      this.isWhitelistManaged_ = isWhitelistManaged;
    });
  },

  /**
   * @param {!Event} e
   * @private
   */
  openAddUserDialog_(e) {
    e.preventDefault();
    this.$.addUserDialog.open();
  },

  /** @private */
  onAddUserDialogClose_() {
    cr.ui.focusWithoutInk(assert(this.$$('#add-user-button a')));
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @private
   * @return {boolean}
   */
  isEditingDisabled_(isOwner, isWhitelistManaged) {
    return !isOwner || isWhitelistManaged;
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @param {boolean} allowGuest
   * @param {boolean} isChild
   * @private
   * @return {boolean}
   */
  isEditingUsersEnabled_(isOwner, isWhitelistManaged, allowGuest, isChild) {
    return isOwner && !isWhitelistManaged && !allowGuest && !isChild;
  },

  /** @return {boolean} */
  shouldHideModifiedByOwnerLabel_() {
    return this.isWhitelistManaged_ || this.isOwner_;
  },
});
