// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'controlled-radio-button',

  behaviors: [
    PrefControlBehavior,
    CrRadioButtonBehavior,
  ],

  observers: [
    'updateDisabled_(pref.enforcement)',
  ],

  /** @private */
  updateDisabled_() {
    this.disabled =
        this.pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showIndicator_() {
    return this.disabled &&
        this.name == Settings.PrefUtil.prefToString(assert(this.pref));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_(e) {
    // Disallow <controlled-radio-button on-click="..."> when disabled.
    e.preventDefault();
    e.stopPropagation();
  },
});
