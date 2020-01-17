// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'plugin-vm-subpage' is the settings subpage for managing Plugin VM.
 */

Polymer({
  is: 'settings-plugin-vm-subpage',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showRemoveConfirmationDialog_: {
      type: Boolean,
      value: false,
    },
  },

  // TODO(juwa@google.com): Navigate back if plugin vm uninstalled.

  /** @private */
  onSharedPathsClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.PLUGIN_VM_SHARED_PATHS);
  },

  /**
   * Shows a confirmation dialog, which if accepted will remove PluginVm.
   * @private
   */
  onRemoveClick_() {
    this.showRemoveConfirmationDialog_ = true;
  },

  /**
   * Hides the remove confirmation dialog.
   * @private
   */
  onRemoveConfirmationDialogClose_: function() {
    this.showRemoveConfirmationDialog_ = false;
    this.$.pluginVmRemoveButton.focus();
  },
});
