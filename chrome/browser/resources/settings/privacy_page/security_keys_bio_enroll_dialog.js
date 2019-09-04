// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * @fileoverview 'settings-security-keys-bio-enroll-dialog' is a dialog for
 * listing, adding, renaming, and deleting biometric enrollments stored on a
 * security key.
 */
Polymer({
  is: 'settings-security-keys-bio-enroll-dialog',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    addButtonVisible_: Boolean,

    /** @private */
    cancelButtonVisible_: Boolean,

    /** @private */
    deleteInProgress_: Boolean,

    /**
     * The ID of the element currently shown in the dialog.
     * @private
     */
    dialogPage_: {
      type: String,
      value: 'initial',
      observer: 'dialogPageChanged_',
    },

    /** @private */
    doneButtonVisible_: Boolean,

    /**
     * The list of enrollments displayed.
     * @private {!Array<!Enrollment>}
     */
    enrollments_: Array,

    /** @private */
    okButtonVisible_: Boolean,
  },

  /** @private {?settings.SecurityKeysBioEnrollProxyImpl} */
  browserProxy_: null,

  /** @private */
  maxSamples_: Number,

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
    this.addWebUIListener(
        'security-keys-bio-enroll-error', this.onError_.bind(this));
    this.addWebUIListener(
        'security-keys-bio-enroll-status', this.onEnrolling_.bind(this));
    this.browserProxy_ = settings.SecurityKeysBioEnrollProxyImpl.getInstance();
    this.browserProxy_.startBioEnroll().then(
        this.collectPIN_.bind(this), () => {});
  },

  /** @private */
  collectPIN_: function() {
    this.dialogPage_ = 'pinPrompt';
    this.$.pin.focus();
  },

  /**
   * @private
   * @param {string} error
   */
  onError_: function(error) {
    this.errorMsg_ = error;
    this.dialogPage_ = 'error';
  },

  /** @private */
  submitPIN_: function() {
    if (!this.$.pin.validate()) {
      return;
    }
    this.browserProxy_.providePIN(this.$.pin.value).then((retries) => {
      if (retries != null) {
        this.$.pin.showIncorrectPINError(retries);
        return;
      }

      this.browserProxy_.enumerateEnrollments().then(
          this.onEnrollments_.bind(this));
    }, () => {});
  },

  /**
   * @private
   * @param {!Array<!Enrollment>} enrollments
   */
  onEnrollments_: function(enrollments) {
    this.enrollments_ = enrollments;
    this.$.enrollmentList.fire('iron-resize');
    this.dialogPage_ = 'enrollments';
  },

  /** @private */
  dialogPageChanged_: function() {
    switch (this.dialogPage_) {
      case 'initial':
        this.addButtonVisible_ = false;
        this.cancelButtonVisible_ = true;
        this.okButtonVisible_ = false;
        this.doneButtonVisible_ = false;
        break;
      case 'pinPrompt':
        this.addButtonVisible_ = false;
        this.cancelButtonVisible_ = true;
        this.okButtonVisible_ = true;
        this.doneButtonVisible_ = false;
        break;
      case 'enrollments':
        this.addButtonVisible_ = true;
        this.cancelButtonVisible_ = false;
        this.okButtonVisible_ = false;
        this.doneButtonVisible_ = true;
        break;
      case 'enroll':
        this.addButtonVisible_ = false;
        this.cancelButtonVisible_ = true;
        this.okButtonVisible_ = false;
        this.doneButtonVisible_ = false;
        break;
      case 'error':
        this.addButtonVisible_ = false;
        this.cancelButtonVisible_ = false;
        this.okButtonVisible_ = false;
        this.doneButtonVisible_ = true;
        break;
      default:
        assertNotReached();
    }
    this.fire('bio-enroll-dialog-ready-for-testing');
  },

  /** @private */
  addButtonClick_: function() {
    assert(this.dialogPage_ == 'enrollments');

    this.maxSamples_ = -1;  // Reset maxSamples_ before enrolling starts.
    this.$.arc.reset();
    this.cancelButtonVisible_ = true;
    this.okButtonVisible_ = false;

    this.dialogPage_ = 'enroll';
    this.browserProxy_.startEnrolling().then(
        this.onEnrolling_.bind(this), () => {});
  },

  /**
   * @private
   * @param {!EnrollmentStatus} response
   */
  onEnrolling_: function(response) {
    if (this.maxSamples_ == -1 && response.status != null) {
      if (response.status == 0) {
        // If the first sample is valid, remaining is one less than max samples
        // required.
        this.maxSamples_ = response.remaining + 1;
      } else {
        // If the first sample failed for any reason (timed out, key full, etc),
        // the remaining number of samples is the max samples required.
        this.maxSamples_ = response.remaining;
      }
    }
    // If 0 samples remain, the enrollment has finished in some state.
    // Currently not checking response['code'] for an error.
    this.$.arc.setProgress(
        100 - (100 * (response.remaining + 1) / this.maxSamples_),
        100 - (100 * response.remaining / this.maxSamples_),
        response.remaining == 0);
    if (response.remaining == 0) {
      this.cancelButtonVisible_ = false;
      this.okButtonVisible_ = true;
    }
    this.fire('bio-enroll-dialog-ready-for-testing');
  },

  /** @private */
  okButtonClick_: function() {
    switch (this.dialogPage_) {
      case 'pinPrompt':
        this.submitPIN_();
        break;
      case 'enroll':
        this.browserProxy_.enumerateEnrollments().then(
            this.onEnrollments_.bind(this), () => {});
        break;
      default:
        assertNotReached();
    }
  },

  /** @private */
  cancel_: function() {
    if (this.dialogPage_ == 'enroll') {
      this.browserProxy_.cancelEnrollment().then(
          this.cancelEnroll_.bind(this), () => {});
    } else {
      this.done_();
    }
  },

  /** @private */
  cancelEnroll_: function() {
    // Cancelling from the enrolling screen redirects to the enrollments
    // list, so request another enumeration to display.
    this.browserProxy_.enumerateEnrollments().then(
        this.onEnrollments_.bind(this), () => {});
  },

  /** @private */
  done_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onDialogClosed_: function() {
    this.browserProxy_.close();
  },

  /**
   * @private
   * @param {!Event} e
   */
  onIronSelect_: function(e) {
    // Prevent this event from bubbling since it is unnecessarily triggering the
    // listener within settings-animated-pages.
    e.stopPropagation();
  },

  /**
   * @private
   * @param {?Array} list
   * @return {boolean} true if the list exists and has items.
   */
  hasSome_: function(list) {
    return !!(list && list.length);
  },

  /**
   * @private
   * @param {!DomRepeatEvent} event
   */
  deleteEnrollment_: function(event) {
    if (this.deleteInProgress_) {
      return;
    }
    this.deleteInProgress_ = true;
    const enrollment = this.enrollments_[event.model.index];
    this.browserProxy_.deleteEnrollment(enrollment.id).then(enrollments => {
      this.deleteInProgress_ = false;
      this.onEnrollments_(enrollments);
    });
  }
});
})();
