// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-nearby-share-subpage' is the settings subpage for managing the
 * Nearby Share feature.
 */

Polymer({
  is: 'settings-nearby-share-subpage',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_(event) {
    this.setPrefValue(
        'nearby_sharing.enabled',
        !this.getPref('nearby_sharing.enabled').value);
    event.stopPropagation();
  },

  /**
   * @param {boolean} state boolean state that determines which string to show
   * @param {string} onstr string to show when state is true
   * @param {string} offstr string to show when state is false
   * @return {string} localized string
   * @private
   */
  getOnOffString_(state, onstr, offstr) {
    return state ? onstr : offstr;
  },
});
