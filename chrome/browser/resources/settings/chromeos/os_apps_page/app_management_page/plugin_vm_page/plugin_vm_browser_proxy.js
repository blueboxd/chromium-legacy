// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * These values should remain consistent with their C++ counterpart
 * (chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h).
 * @enum {number}
 */
const PermissionType = {
  CAMERA: 0,
  MICROPHONE: 1,
};

/**
 * @typedef {{permissionType: !PermissionType,
 *            proposedValue: boolean}}
 */
let PermissionSetting;

/**
 * @fileoverview A helper object used by the Plugin VM section
 * to manage the Plugin VM.
 */
cr.define('settings', function() {
  /** @interface */
  class PluginVmBrowserProxy {
    /**
     * @param {!Array<string>} paths Paths to sanitze.
     * @return {!Promise<!Array<string>>} Text to display in UI.
     */
    getPluginVmSharedPathsDisplayText(paths) {}

    /**
     * @param {string} vmName VM to stop sharing path with.
     * @param {string} path Path to stop sharing.
     */
    removePluginVmSharedPath(vmName, path) {}

    /**
     * @param {!PermissionSetting} permissionSetting The proposed change to
     *     permissions
     * @return {!Promise<boolean>} Whether Plugin VM needs to be relaunched for
     *     permissions to take effect.
     */
    wouldPermissionChangeRequireRelaunch(permissionSetting) {}

    /**
     * @param {!PermissionSetting} permissionSetting The change to make to the
     *     permissions
     */
    setPluginVmPermission(permissionSetting) {}
  }

  /** @implements {settings.PluginVmBrowserProxy} */
  class PluginVmBrowserProxyImpl {
    /** @override */
    getPluginVmSharedPathsDisplayText(paths) {
      return cr.sendWithPromise('getPluginVmSharedPathsDisplayText', paths);
    }

    /** @override */
    removePluginVmSharedPath(vmName, path) {
      chrome.send('removePluginVmSharedPath', [vmName, path]);
    }

    /** @override */
    wouldPermissionChangeRequireRelaunch(permissionSetting) {
      return cr.sendWithPromise(
          'wouldPermissionChangeRequireRelaunch',
          permissionSetting.permissionType, permissionSetting.proposedValue);
    }

    /** @override */
    setPluginVmPermission(permissionSetting) {
      chrome.send(
          'setPluginVmPermission',
          [permissionSetting.permissionType, permissionSetting.proposedValue]);
    }
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(PluginVmBrowserProxyImpl);

  // #cr_define_end
  return {
    PluginVmBrowserProxy: PluginVmBrowserProxy,
    PluginVmBrowserProxyImpl: PluginVmBrowserProxyImpl,
  };
});
