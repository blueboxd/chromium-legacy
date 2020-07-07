// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-dlc-subpage' is the Downloaded Content subpage.
 */
cr.define('settings', function() {
  Polymer({
    is: 'os-settings-dlc-subpage',

    behaviors: [
      WebUIListenerBehavior,
    ],

    properties: {
      /**
       * The list of DLC Metadata.
       * @private {!Array<!settings.DlcMetadata>}
       */
      dlcList_: {
        type: Array,
        value: [],
      },
    },

    /** @private {?settings.DevicePageBrowserProxy} */
    browserProxy_: null,

    /** @override */
    created() {
      this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
    },

    /** @override */
    attached() {
      this.addWebUIListener(
          'dlc-list-changed', this.onDlcListChanged_.bind(this));
      this.browserProxy_.notifyDlcSubpageReady();
    },

    /**
     * Handles event when remove is clicked.
     * @param {!Event} e The click event with an added attribute 'data-dlc-id'.
     * @private
     */
    onRemoveDlcClick_(e) {
      const item = this.$.list.items[e.target.dataset.index];
      item.disabled = true;
      this.browserProxy_.purgeDlc(item.id);
    },

    /**
     * @param {!Array<!settings.DlcMetadata>} dlcList A list of DLC metadata.
     * @private
     */
    onDlcListChanged_(dlcList) {
      this.dlcList_ = dlcList;
    },

    /**
     * @param {string} description The DLC description string.
     * @return {boolean} Whether to include the tooltip description.
     * @private
     */
    includeTooltip_(description) {
      return !!description;
    },
  });

  // #cr_define_end
  return {};
});
