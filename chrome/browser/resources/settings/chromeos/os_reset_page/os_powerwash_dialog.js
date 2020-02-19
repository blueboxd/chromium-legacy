// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-powerwash-dialog' is a dialog shown to request confirmation
 * from the user for a device reset (aka powerwash).
 */
Polymer({
  is: 'os-settings-powerwash-dialog',

  properties: {
    requestTpmFirmwareUpdate: {
      type: Boolean,
      value: false,
    }
  },

  /** @override */
  attached() {
    settings.OsResetBrowserProxyImpl.getInstance().onPowerwashDialogShow();
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onRestartTap_() {
    settings.recordSettingChange();
    settings.LifetimeBrowserProxyImpl.getInstance().factoryReset(
        this.requestTpmFirmwareUpdate);
  },
});
