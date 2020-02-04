// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'plugin-vm-page' is the settings page for Plugin VM.
 */

Polymer({
  is: 'settings-plugin-vm-page',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    allowPluginVm_: Boolean,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.PLUGIN_VM_DETAILS) {
          map.set(settings.routes.PLUGIN_VM_DETAILS.path, '#plugin-vm');
        }
        if (settings.routes.PLUGIN_VM_SHARED_PATHS) {
          map.set(settings.routes.PLUGIN_VM_SHARED_PATHS.path, '#plugin-vm');
        }
        return map;
      },
    },
  },

  /** @override */
  attached: function() {
    this.allowPluginVm_ = loadTimeData.valueExists('allowPluginVm') &&
        loadTimeData.getBoolean('allowPluginVm');
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSubpageClick_(event) {
    if (this.getPref('plugin_vm.image_exists').value) {
      settings.Router.getInstance().navigateTo(
          settings.routes.PLUGIN_VM_DETAILS);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableClick_(event) {
    settings.PluginVmBrowserProxyImpl.getInstance()
        .requestPluginVmInstallerView();
    event.stopPropagation();
  },
});
