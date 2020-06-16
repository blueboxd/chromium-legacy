// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('account_migration_welcome', function() {
  'use strict';

  Polymer({
    is: 'account-migration-welcome',

    behaviors: [
      I18nBehavior,
    ],

    properties: {
      title_: {
        type: String,
        value: '',
      },
      body_: {
        type: String,
        value: '',
      },
    },

    /** @private {String} */
    userEmail_: String,
    /** @private {AccountManagerBrowserProxy} */
    browserProxy_: Object,

    /** @override */
    ready() {
      this.browserProxy_ =
          account_manager.AccountManagerBrowserProxyImpl.getInstance();

      const dialogArgs = chrome.getVariableValue('dialogArguments');
      if (!dialogArgs) {
        // Only if the user navigates to the URL
        // chrome://account-migration-welcome to debug.
        console.warn('No arguments were provided to the dialog.');
        return;
      }
      const args = JSON.parse(dialogArgs);
      assert(args);
      assert(args.email);
      this.userEmail_ = args.email;

      this.title_ = loadTimeData.getStringF('welcomeTitle', this.userEmail_);
      this.body_ = this.getWelcomeMessage_();
    },

    /** @private */
    getWelcomeMessage_() {
      const validNodeFn = (node, value) => node.tagName === 'A';
      return this.i18nAdvanced('welcomeMessage', {
        substitutions: [
          this.userEmail_, loadTimeData.getString('accountManagerLearnMoreUrl')
        ],
        attrs: {'id': validNodeFn, 'href': validNodeFn}
      });
    },

    /** @private */
    closeDialog_() {
      this.browserProxy_.closeDialog();
    },

    /** @private */
    reauthenticateAccount_() {
      this.browserProxy_.reauthenticateAccount(this.userEmail_);
    },
  });
});
