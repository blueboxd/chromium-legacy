// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'plugin-vm-subpage' is the settings subpage for managing Plugin VM.
 */

Polymer({
  is: 'settings-plugin-vm-subpage',

  behaviors: [PrefsBehavior],

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

    /**
     * Whether the toggle to share the camera with PluginVm should be shown.
     * @private {boolean}
     */
    showPluginVmCamera_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showPluginVmCamera');
      },
    },
  },

  observers: [
    'onPluginVmImageExistsChanged_(prefs.plugin_vm.image_exists.value)',
  ],

  /**
   * @param {boolean} exists
   * @private
   */
  onPluginVmImageExistsChanged_(exists) {
    if (!exists) {
      settings.Router.getInstance().navigateTo(settings.routes.PLUGIN_VM);
    }
  },

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
