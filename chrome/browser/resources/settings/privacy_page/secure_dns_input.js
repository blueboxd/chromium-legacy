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
   * trigger an event with the result. If the value is valid, also attempt a
   * test query. Show an error message if the tested value is still the most
   * recent value, is non-empty, and was either invalid or failed the test
   * query.
   * @private
   */
  validate: async function() {
    this.showError_ = false;
    const valueToValidate = this.value;
    const firstTemplate =
        await this.browserProxy_.validateCustomDnsEntry(valueToValidate);
    const successfulProbe = firstTemplate &&
        await this.browserProxy_.probeCustomDnsTemplate(firstTemplate);
    // If the group was invalid or the first template doesn't successfully
    // answer a probe query, show an error as long as the input field value
    // hasn't changed and is non-empty.
    if (valueToValidate === this.value && this.value !== '' &&
        !successfulProbe) {
      this.errorText_ = loadTimeData.getString(
          firstTemplate ? 'secureDnsCustomConnectionError' :
                          'secureDnsCustomFormatError');
      this.showError_ = true;
    }
    this.fire(
        'value-update', {isValid: !!firstTemplate, text: valueToValidate});
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
