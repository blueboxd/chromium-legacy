// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview `secure-dns-input` is a single-line text field that is used
 * with the secure DNS setting to configure custom servers. It is based on
 * `home-url-input`.
 */
Polymer({
  is: 'secure-dns-input',

  properties: {
    /*
     * The value of the input field.
     */
    value: String,

    /*
     * Whether |errorText| should be displayed beneath the input field.
     * @private
     */
    showError_: Boolean,

    /**
     * The error text to display beneath the input field when |showError_| is
     * true.
     * @private
     */
    errorText_: String,
  },

  /** @private {?settings.PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();
  },

  /**
   * This function ensures that while the user is entering input, especially
   * after pressing Enter, the input is not prematurely marked as invalid.
   * @private
   */
  onInput_: function() {
    this.showError_ = false;
  },

  /**
   * When the custom input field loses focus, validate the current value and
   * trigger an event with the result. Show an error message if the validated
   * value is still the most recent value, is invalid, and is non-empty.
   */
  validate: function() {
    const valueToValidate = this.value;
    this.browserProxy_.validateCustomDnsEntry(valueToValidate).then(valid => {
      this.showError_ =
          valueToValidate === this.value && !valid && valueToValidate !== '';
      if (this.showError_) {
        this.errorText_ = loadTimeData.getString('secureDnsCustomFormatError');
      }

      this.fire('value-update', {isValid: valid, text: valueToValidate});
    });
  },

  /**
   * Focus the custom dns input field.
   */
  focus: function() {
    this.$.input.focus();
  },

  /**
   * Returns whether an error is being shown.
   * @return {boolean}
   */
  isInvalid: function() {
    return !!this.showError_;
  },
});
