// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/*
 * Fit to page and fit to paper options will only be displayed for PDF
 * documents. If the custom option is selected, an additional input field will
 * appear to enter the custom scale factor.
 */
Polymer({
  is: 'print-preview-scaling-settings',

  behaviors: [SettingsBehavior, print_preview.SelectBehavior],

  properties: {
    disabled: {
      type: Boolean,
      observer: 'onDisabledChanged_',
    },

    isPdf: Boolean,

    /** @private {string} */
    currentValue_: {
      type: String,
      observer: 'onInputChanged_',
    },

    /** @private {boolean} */
    customSelected_: {
      type: Boolean,
      computed: 'computeCustomSelected_(settingKey_, ' +
          'settings.scalingType.*, settings.scalingTypePdf.*)',
    },

    /** @private {boolean} */
    inputValid_: Boolean,

    /** @private {boolean} */
    dropdownDisabled_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    settingKey_: {
      type: String,
      computed: 'computeSettingKey_(isPdf)',
    },

    /** Mirroring the enum so that it can be used from HTML bindings. */
    ScalingValue: {
      type: Object,
      value: print_preview.ScalingType,
    },
  },

  observers: [
    'onScalingTypeSettingChanged_(settingKey_, settings.scalingType.value, ' +
        'settings.scalingTypePdf.value)',
    'onScalingSettingChanged_(settings.scaling.value)',
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

  onProcessSelectChange: function(value) {
    const isCustom = value === print_preview.ScalingType.CUSTOM.toString();
    if (isCustom && !this.customScalingSettingSet_) {
      this.userSelectedCustomScaling_ = true;
    } else {
      this.customScalingSettingSet_ = false;
    }

    const valueAsNumber = parseInt(value, 10);
    if (isCustom || value === print_preview.ScalingType.DEFAULT.toString()) {
      this.setSetting('scalingType', valueAsNumber);
    }
    if (this.isPdf ||
        this.getSetting('scalingTypePdf').value ===
            print_preview.ScalingType.DEFAULT ||
        this.getSetting('scalingTypePdf').value ===
            print_preview.ScalingType.CUSTOM) {
      this.setSetting('scalingTypePdf', valueAsNumber);
    }

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

  /**
   * Updates the input string when scaling setting is set.
   * @private
   */
  onScalingSettingChanged_: function() {
    const value = /** @type {string} */ (this.getSetting('scaling').value);
    this.lastValidScaling_ = value;
    this.currentValue_ = value;
  },

  /** @private */
  onScalingTypeSettingChanged_: function() {
    if (!this.settingKey_) {
      return;
    }

    const value = /** @type {!print_preview.ScalingType} */
        (this.getSettingValue(this.settingKey_));
    if (value !== print_preview.ScalingType.CUSTOM) {
      this.updateScalingToValid_();
    } else {
      this.customScalingSettingSet_ = true;
    }
    this.selectedValue = value.toString();
  },

  /**
   * Updates scaling settings based on the validity and current value of the
   * scaling input.
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
    return !!this.settingKey_ &&
        this.getSettingValue(this.settingKey_) ===
        print_preview.ScalingType.CUSTOM;
  },

  /**
   * @return {string} The key of the appropriate scaling setting.
   * @private
   */
  computeSettingKey_: function() {
    return this.isPdf ? 'scalingTypePdf' : 'scalingType';
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
