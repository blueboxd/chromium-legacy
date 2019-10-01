// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-margins-settings',

  behaviors: [SettingsBehavior, print_preview.SelectBehavior],

  properties: {
    disabled: Boolean,

    /** Mirroring the enum so that it can be used from HTML bindings. */
    MarginsValue: Object,
  },

  observers: ['onMarginsSettingChange_(settings.margins.value)'],

  /** @override */
  ready: function() {
    this.MarginsValue = print_preview.MarginsTypeValue;
  },

  /**
   * @param {*} newValue The new value of the margins setting.
   * @private
   */
  onMarginsSettingChange_: function(newValue) {
    this.selectedValue =
        /** @type {!print_preview.MarginsTypeValue} */ (newValue).toString();
  },

  /** @param {string} value The new select value. */
  onProcessSelectChange: function(value) {
    this.setSetting('margins', parseInt(value, 10));
  },

  /**
   * @param {boolean} globallyDisabled Value of the |disabled| property.
   * @param {number} pagesPerSheet Number of pages per sheet.
   * @return {boolean} Whether the margins settings button should be disabled.
   * @private
   */
  getMarginsSettingsDisabled_: function(globallyDisabled, pagesPerSheet) {
    return globallyDisabled || pagesPerSheet > 1;
  },
});
