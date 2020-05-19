// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('account_manager_error', function() {
  'use strict';

  /**
   * Keep in sync with
   * chrome/browser/ui/webui/signin/inline_login_handler_dialog_chromeos.cc
   * @enum {number}
   */
  const AccountManagerErrorType = {
    SECONDARY_ACCOUNTS_DISABLED: 0,
    CHILD_USER_ARC_DISABLED: 1,
  };

  Polymer({
    is: 'account-manager-error',

    behaviors: [
      I18nBehavior,
    ],

    properties: {
      errorTitle_: {
        type: String,
        value: '',
      },
      errorMessage_: {
        type: String,
        value: '',
      },
      imageUrl_: {
        type: String,
        value: '',
      },
    },

    /** @override */
    ready() {
      const errorType = this.getErrorType_();
      if (errorType === AccountManagerErrorType.CHILD_USER_ARC_DISABLED) {
        this.errorTitle_ = this.i18n('childUserArcDisabledErrorTitle');
        this.errorMessage_ = this.i18n('childUserArcDisabledErrorMessage');
        this.imageUrl_ = 'family_link_logo.svg';
      } else {
        this.errorTitle_ = this.i18n('secondaryAccountsDisabledErrorTitle');
        this.errorMessage_ = this.i18n('secondaryAccountsDisabledErrorMessage');
      }
      document.title = this.errorTitle_;
    },

    /**
     * @returns {number}
     */
    getErrorType_() {
      const dialogArgs = chrome.getVariableValue('dialogArguments');
      assert(dialogArgs);
      const args = JSON.parse(dialogArgs);
      assert(args);
      assert(Number.isInteger(args.errorType));
      return args.errorType;
    },

    closeDialog_() {
      chrome.send('closeDialog');
    },
  });
});
