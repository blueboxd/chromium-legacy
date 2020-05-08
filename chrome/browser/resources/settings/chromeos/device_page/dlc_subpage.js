// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-dlc-subpage' is the Downloaded Content subpage.
 */
cr.define('settings', function() {
  /**
   * TODO(hsuregan): Delete when DLC subpage is finished.
   * Whether to fetch fake DLC list.
   * @type {boolean}
   */
  const USE_FAKE_DLC_LIST = true;

  /**
   * TODO(hsuregan): Delete when DLC subpage is finished.
   * Fake function to simulate async DevicePageBrowserProxy getDlcList().
   * @return {!Promise<!Array<!settings.DlcMetadata>>} A list of DLC metadata.
   */
  function getFakeDlcList() {
    /**
     * @param {number} max The upper bound integer.
     * @return {number} A random in the range [0, max).
     */
    function getRandomInt(max) {
      return Math.floor(Math.random() * Math.floor(max));
    }

    const fakeRandomResults =
        [...Array(getRandomInt(getRandomInt(40))).keys()].map((idx) => {
          const dlcDescription = getRandomInt(2) ?
              `fake description ${idx} `.repeat(getRandomInt(500)) :
              '';
          return {
            id: `fake id ${idx}`,
            name: `fake name ${idx}`,
            description: dlcDescription,
            diskUsageLabel: `fake diskUsageLabel`,
          };
        });

    console.log(fakeRandomResults);
    return new Promise(resolve => {
      setTimeout(() => {
        resolve(fakeRandomResults);
      }, 0);
    });
  }

  Polymer({
    is: 'os-settings-dlc-subpage',

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
      if (USE_FAKE_DLC_LIST) {
        getFakeDlcList().then(this.onDlcListChanged_.bind(this));
        return;
      }

      this.browserProxy_.getDlcList().then(this.onDlcListChanged_.bind(this));
    },

    /**
     * Handles event when remove is clicked.
     * @param {!Event} e The click event with an added attribute 'data-dlc-id'.
     * @private
     */
    onRemoveDlcClick_(e) {
      const removeButton = this.shadowRoot.querySelector(`#${e.target.id}`);
      removeButton.disabled = true;
      this.browserProxy_.purgeDlc(e.target.getAttribute('data-dlc-id'))
          .then(success => {
            if (success) {
              // No further action is necessary since the list will change via
              // onDlcListChanged_().
              return;
            }
            console.log(`Unable to purge DLC with ID ${e.target.id}`);
            removeButton.disabled = false;
          });
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
