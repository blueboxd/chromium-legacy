// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/** @enum {number} */
const ScalingValue = {
  DEFAULT: 0,
  FIT_TO_PAGE: 1,
  CUSTOM: 2,
};

/*
 * When fit to page is available, the checkbox and input interact as follows:
 * 1. When checkbox is checked, the fit to page scaling value is displayed in
 * the input. The error message is cleared if it was present.
 * 2. When checkbox is unchecked, the most recent valid scale value is restored.
 * 3. If the input is modified while the checkbox is checked, the checkbox will
 * be unchecked automatically, regardless of the validity of the new value.
 */
Polymer({
  is: 'print-preview-scaling-settings',

  behaviors: [SettingsBehavior, print_preview.SelectBehavior],

  properties: {
    disabled: {
      type: Boolean,
      observer: 'onDisabledChanged_',
    },

    /** @private {string} */
    currentValue_: {
      type: String,
      observer: 'onInputChanged_',
    },

    /** @private {boolean} */
    customSelected_: {
      type: Boolean,
      computed: 'computeCustomSelected_(settings.customScaling.*, ' +
          'settings.fitToPage.*)',
    },

    /** @private {boolean} */
    inputValid_: Boolean,

    /** @private {boolean} */
    dropdownDisabled_: {
      type: Boolean,
      value: false,
    },

    /** Mirroring the enum so that it can be used from HTML bindings. */
    ScalingValue: Object,
  },

  observers: [
    'onFitToPageSettingChange_(settings.fitToPage.value)',
    'onScalingSettingChanged_(settings.scaling.value)',
    'onCustomScalingSettingChanged_(settings.customScaling.value)',
  ],

  /** @private {string} */
  lastValidScaling_: '',

  /**
   * Whether the custom scaling setting has been set to true, but the custom
   * input has not yet been expanded. Used to determine whether changes in the
   * dropdown are due to user input or sticky settings.
   * @private {boolean}
   */
  customScalingSettingSet_: false,

  /**
   * Whether the user has selected custom scaling in the dropdown, but the
   * custom input has not yet been expanded. Used to determine whether to
   * auto-focus the custom input.
   * @private {boolean}
   */
  userSelectedCustomScaling_: false,

  /** @override */
  ready: function() {
    this.ScalingValue = ScalingValue;
  },

  onProcessSelectChange: function(value) {
    if (value === ScalingValue.FIT_TO_PAGE.toString()) {
      this.setSetting('fitToPage', true);
      return;
    }

    const fitToPageAvailable = this.getSetting('fitToPage').available;
    if (fitToPageAvailable) {
      this.setSetting('fitToPage', false);
    }
    const isCustom = value === ScalingValue.CUSTOM.toString();
    if (isCustom && !this.customScalingSettingSet_) {
      this.userSelectedCustomScaling_ = true;
    } else {
      this.customScalingSettingSet_ = false;
    }
    this.setSetting('customScaling', isCustom);
    if (isCustom) {
      this.setSetting('scaling', this.currentValue_);
    }
  },

  /** @private */
  updateScalingToValid_: function() {
    if (!this.getSetting('scaling').valid) {
      this.currentValue_ = this.lastValidScaling_;
    } else {
      this.lastValidScaling_ = this.currentValue_;
    }
  },

  /** @private */
  onFitToPageSettingChange_: function() {
    if (!this.getSettingValue('fitToPage') ||
        !this.getSetting('fitToPage').available) {
      return;
    }

    this.updateScalingToValid_();
    this.selectedValue = ScalingValue.FIT_TO_PAGE.toString();
  },

  /** @private */
  onCustomScalingSettingChanged_: function() {
    if (this.getSettingValue('fitToPage') &&
        this.getSetting('fitToPage').available) {
      return;
    }

    const isCustom =
        /** @type {boolean} */ (this.getSetting('customScaling').value);
    if (!isCustom) {
      this.updateScalingToValid_();
    } else {
      this.customScalingSettingSet_ = true;
    }
    this.selectedValue = isCustom ? ScalingValue.CUSTOM.toString() :
                                    ScalingValue.DEFAULT.toString();
  },

  /**
   * Updates the input string when scaling setting is set.
   * @private
   */
  onScalingSettingChanged_: function() {
    const value = /** @type {string} */ (this.getSetting('scaling').value);
    this.lastValidScaling_ = value;
    this.currentValue_ = value;
  },

  /**
   * Updates scaling and fit to page settings based on the validity and current
   * value of the scaling input.
   * @private
   */
  onInputChanged_: function() {
    this.setSettingValid('scaling', this.inputValid_);

    if (this.currentValue_ !== '' && this.inputValid_ &&
        this.currentValue_ !== this.getSettingValue('scaling')) {
      this.setSetting('scaling', this.currentValue_);
    }
  },

  /** @private */
  onDisabledChanged_: function() {
    this.dropdownDisabled_ = this.disabled && this.inputValid_;
  },

  /**
   * @return {boolean} Whether the input should be disabled.
   * @private
   */
  inputDisabled_: function() {
    return !this.customSelected_ || this.dropdownDisabled_;
  },

  /**
   * @return {boolean} Whether the custom scaling option is selected.
   * @private
   */
  computeCustomSelected_: function() {
    return /** @type {boolean} */ (this.getSettingValue('customScaling')) &&
        (!this.getSetting('fitToPage').available ||
         !(/** @type {boolean} */ (this.getSettingValue('fitToPage'))));
  },

  /** @private */
  onCollapseChanged_: function() {
    if (this.customSelected_ && this.userSelectedCustomScaling_) {
      this.$$('print-preview-number-settings-section').getInput().focus();
    }
    this.customScalingSettingSet_ = false;
    this.userSelectedCustomScaling_ = false;
  },
});
