// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cookies-page' is the settings page containing cookies
 * settings.
 */

(function() {
/**
 * Enumeration of all cookies radio controls.
 * @enum {string}
 */
const CookiesControl = {
  ALLOW_ALL: 'allow-all',
  BLOCK_THIRD_INCOGNITO: 'block-third-incognito',
  BLOCK_THIRD: 'block-third',
  BLOCK_ALL: 'block-all',
};

Polymer({
  is: 'settings-cookies-page',

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Valid cookie states.
     * @private
     */
    cookiesControlEnum_: {
      type: Object,
      value: CookiesControl,
    },

    /** @private {!CookiesControl} */
    selectSafeBrowsingRadio_: {
      type: String,
      value: CookiesControl.BLOCK_THIRD_INCOGNITO,
    },

    /**
     * @private {!settings.ContentSettingsTypes}
     */
    ContentSettingsTypes: {type: Object, value: settings.ContentSettingsTypes},
  },
});
})();
