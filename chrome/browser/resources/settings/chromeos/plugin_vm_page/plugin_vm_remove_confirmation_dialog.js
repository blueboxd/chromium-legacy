// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-plugin-vm-remove-confirmation-dialog' is a component
 * warning the user that remove plugin vm removes their vm's data and settings.
 */
Polymer({
  is: 'settings-plugin-vm-remove-confirmation-dialog',

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelClick_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onContinueClick_: function() {
    settings.PluginVmBrowserProxyImpl.getInstance().removePluginVm();
    this.$.dialog.close();
  },
});
