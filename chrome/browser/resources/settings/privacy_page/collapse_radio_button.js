// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-collapse-radio-button',

  behaviors: [
    CrRadioButtonBehavior,
  ],

  properties: {
    expanded: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /**
     * Which indicator type to show (or NONE).
     * @type {CrPolicyIndicatorType}
     */
    policyIndicatorType: {
      type: String,
      value: CrPolicyIndicatorType.NONE,
    },

    noCollapse: Boolean,

    label: String,

    subLabel: {
      type: String,
      value: '',  // Allows the $hidden= binding to run without being set.
    },
  },

  observers: ['onCheckedChanged_(checked)'],

  /** @private */
  onCheckedChanged_() {
    this.expanded = this.checked;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorClick_(e) {
    // Prevent interacting with the indicator changing anything when disabled.
    e.preventDefault();
    e.stopPropagation();
  },
});
