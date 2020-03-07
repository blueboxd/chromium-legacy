// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cookies-page' is the settings page containing cookies
 * settings.
 */

Polymer({
  is: 'settings-cookies-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * @private {!settings.ContentSettingsTypes}
     */
    ContentSettingsTypes: {type: Object, value: settings.ContentSettingsTypes},
  },
});
