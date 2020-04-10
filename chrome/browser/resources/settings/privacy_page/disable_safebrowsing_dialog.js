// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'disable-safebrowsing-dialog' makes sure users want to disable safebrowsing.
 */
Polymer({
  is: 'settings-disable-safebrowsing-dialog',

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @return {boolean} Whether the user confirmed the dialog. */
  wasConfirmed() {
    return /** @type {!CrDialogElement} */ (this.$.dialog)
               .getNative()
               .returnValue == 'success';
  },

  /** @private */
  onDialogCancel_() {
    this.$.dialog.cancel();
  },

  /** @private */
  onDialogConfirm_() {
    this.$.dialog.close();
  },
});
