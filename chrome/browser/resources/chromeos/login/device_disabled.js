// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

Polymer({
  is: 'device-disabled',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: ['setSerialNumberAndEnrollmentDomain', 'setMessage'],

  properties: {
    /**
     * Device serial number
     */
    serial_: {
      type: String,
      value: '',
    },

    /**
     * Enrollment domain (can be empty)
     */
    enrollmentDomain_: {
      type: String,
      value: '',
    },

    /**
     * Admin message (external data, non-html-safe).
     */
    message_: {
      type: String,
      value: '',
    },

  },


  ready() {
    this.initializeLoginScreen('DeviceDisabledScreen', {
      resetAllowed: false,
    });
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.BLOCKING;
  },

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.$.dialog;
  },

  /**
   * Updates the explanation shown to the user. The explanation will indicate
   * the device serial number and that it is owned by |enrollment_domain|. If
   * |enrollment_domain| is null or empty, a generic explanation will be used
   * instead that does not reference any domain.
   * @param {string} serial_number The serial number of the device.
   * @param {string} enrollment_domain The domain that owns the device.
   */
  setSerialNumberAndEnrollmentDomain(serial_number, enrollment_domain) {
    this.serial_ = serial_number;
    this.enrollmentDomain_ = enrollment_domain;
  },

  /**
   * Sets the message to show to the user.
   * @param {string} message The message to show to the user.
   */
  setMessage(message) {
    this.message_ = message;
  },

  disabledText_(locale, serial, domain) {
    if (domain) {
      return this.i18n('deviceDisabledExplanationWithDomain', serial, domain);
    }
    return this.i18n('deviceDisabledExplanationWithoutDomain', serial);
  },
});
