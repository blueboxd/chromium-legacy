// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-edit-dialog' is the dialog that allows showing a
 *     saved password.
 */

(function() {

Polymer({
  is: 'password-edit-dialog',

  behaviors: [ShowPasswordBehavior],

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  },

  /**
   * Handler for tapping the 'done' button. Should just dismiss the dialog.
   * @private
   */
  onActionButtonTap_() {
    this.close();
  },

  /** Manually de-select texts for readonly inputs. */
  onInputBlur_() {
    this.shadowRoot.getSelection().removeAllRanges();
  },
});
})();
