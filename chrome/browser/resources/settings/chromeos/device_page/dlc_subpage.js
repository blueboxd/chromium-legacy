// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-dlc-subpage' is the Downloaded Content subpage.
 */
Polymer({
  is: 'os-settings-dlc-subpage',

  properties: {
    /**
     * The list of DLC Metadata.
     * @private {!Array<!settings.DlcMetadata>}
     */
    dlcList_: Array,
  },

  /** @private {?settings.DevicePageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
  },

  /** override */
  attached() {
    this.browserProxy_.getDlcList().then(this.onDlcListChange_.bind(this));
  },

  /**
   * @param {!Array<!settings.DlcMetadata>} dlcList A list of DLC metadata.
   * @private
   */
  onDlcListChange_(dlcList) {
    this.dlcList_ = dlcList;
  },
});
