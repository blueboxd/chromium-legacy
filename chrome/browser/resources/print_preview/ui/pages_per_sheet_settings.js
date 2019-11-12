// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import {MarginsType} from '../data/margins.js';
import './print_preview_shared_css.js';
import {SelectBehavior} from './select_behavior.js';
import {SettingsBehavior} from './settings_behavior.js';
import './settings_section.js';

Polymer({
  is: 'print-preview-pages-per-sheet-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, SelectBehavior],

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
    this.setSetting('margins', MarginsType.DEFAULT);
  },

  /** @param {string} value The new select value. */
  onProcessSelectChange: function(value) {
    this.setSetting('pagesPerSheet', parseInt(value, 10));
  },
});
