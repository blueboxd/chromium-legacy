// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'app-management-plugin-vm-permission-dialog' is a component
 * that alerts the user when Plugin Vm needs to be relaunched in order for
 * changes to the permission settings to take effect.
 */
Polymer({
  is: 'app-management-plugin-vm-permission-dialog',

  properties: {
    /**
     * An attribute indicating the type of permission that is being
     * changed and what value to change it to.
     * {!PermissionSetting}
     */
    pendingPermissionChange: Object,

    /**
     * If the last permission change should be reset.
     * {boolean}
     */
    resetPermissionChange: {
      type: Boolean,
      notify: true,
    },

    /** @private {string} */
    dialogText_: {
      type: String,
      computed: 'getDialogText_()',
    },

  },
  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.resetPermissionChange = true;
    this.$.dialog.close();
  },

  /** @private */
  onRelaunchTap_() {
    const proxy = settings.PluginVmBrowserProxyImpl.getInstance();
    // Reinitializing PermissionSetting Object to keep the closure compiler
    // happy.
    proxy.setPluginVmPermission({
      permissionType: this.pendingPermissionChange.permissionType,
      proposedValue: this.pendingPermissionChange.proposedValue
    });
    proxy.relaunchPluginVm();
    this.$.dialog.close();
  },

  /** @private */
  getDialogText_() {
    switch (this.pendingPermissionChange.permissionType) {
      case PermissionType.CAMERA:
        return loadTimeData.getString('pluginVmPermissionDialogCameraLabel');
      case PermissionType.MICROPHONE:
        return loadTimeData.getString(
            'pluginVmPermissionDialogMicrophoneLabel');
      default:
        assertNotReached();
    }
  },
});
