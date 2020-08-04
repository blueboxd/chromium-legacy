// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */

Polymer({
  is: 'os-settings-privacy-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether to show the Suggested Content toggle.
     * @private
     */
    showSuggestedContentToggle_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('suggestedContentToggleEnabled');
      },
    },
  },

});
