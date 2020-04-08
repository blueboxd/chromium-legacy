// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-smart-inputs-page' is the settings sub-page
 * to provide users with assistive or expressive input options.
 */

Polymer({
  is: 'os-settings-smart-inputs-page',

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

    /** @private */
    allowAssistivePersonalInfo_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowAssistivePersonalInfo');
      },
    }
  },

  /**
   * Opens Chrome browser's autofill manage addresses setting page.
   * @private
   */
  onManagePersonalInfoClick_() {
    window.open('chrome://settings/addresses');
  },
});
