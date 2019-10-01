// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-pages-per-sheet-settings',

  behaviors: [SettingsBehavior, print_preview.SelectBehavior],

  properties: {
    disabled: Boolean,
  },

  observers: ['onPagesPerSheetSettingChange_(settings.pagesPerSheet.value)'],

  /**
   * @param {*} newValue The new value of the pages per sheet setting.
   * @private
   */
  onPagesPerSheetSettingChange_: function(newValue) {
    this.selectedValue = /** @type {number} */ (newValue).toString();
    this.setSetting('margins', print_preview.MarginsTypeValue.DEFAULT);
  },

  /** @param {string} value The new select value. */
  onProcessSelectChange: function(value) {
    this.setSetting('pagesPerSheet', parseInt(value, 10));
  },
});
