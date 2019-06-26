// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'confirm-password-change' is a dialog so that the user can
 * either confirm their old password, or confirm their new password (twice),
 * or both, as part of an in-session password change.
 */

// TODO(https://crbug.com/930109): Logic is not done. Need to add logic to
// show a spinner, to show only some of the password fields, to show errors,
// and to handle clicks on the save button.

Polymer({
  is: 'confirm-password-change',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /** @private */
  cancel_: function() {
    this.$.dialog.cancel();
  },
});
