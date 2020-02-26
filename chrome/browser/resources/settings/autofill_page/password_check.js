// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-password-check',

  behaviors: [I18nBehavior],

  properties: {
    /** @private */
    passwordLeakCount_: {
      type: Number,
      value: 0,
    },

    /** @private */
    lastCompletedCheck_: Date,
  },

  /**
   * @type {PasswordManagerProxy}
   * @private
   */
  passwordManager_: null,

  /** @override */
  attached() {
    // It's just a placeholder at the moment.
    this.passwordLeakCount_ = 5;

    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();
  },

  /**
   * Start/Stop bulk password check.
   * @private
   */
  onPasswordCheckButtonClick_() {
    // TODO(https://crbug.com/1047726): By click on this button user should be
    // able to 'Cancel' current check.
    this.passwordManager_.startBulkPasswordCheck();
  },

  getLeakedPasswordsCount_() {
    return this.i18n('checkPasswordLeakCount', this.passwordLeakCount_);
  },

  getLastCompletedCheck_() {
    // TODO(https://crbug.com/1047726): use lastCompletedCheck_ to return proper
    // passed time from the last password check.
    return '5 min ago';
  },
});
