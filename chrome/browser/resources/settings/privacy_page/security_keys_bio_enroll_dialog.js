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
    cancelButtonDisabled_: Boolean,

    /** @private */
    cancelButtonVisible_: Boolean,

    /** @private */
    confirmButtonDisabled_: Boolean,

    /** @private */
    confirmButtonVisible_: Boolean,

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
    recentEnrollmentName_: String,
  },

  /** @private {?settings.SecurityKeysBioEnrollProxyImpl} */
  browserProxy_: null,

  /** @private {number} */
  maxSamples_: -1,

  /** @private {string} */
  recentEnrollmentId_: '',

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
    this.addWebUIListener(
        'security-keys-bio-enroll-error', this.onError_.bind(this));
    this.addWebUIListener(
        'security-keys-bio-enroll-status', this.onEnrolling_.bind(this));
    this.browserProxy_ = settings.SecurityKeysBioEnrollProxyImpl.getInstance();
    this.browserProxy_.startBioEnroll().then(() => {
      this.collectPIN_();
    });
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
      this.confirmButtonDisabled_ = false;
      return;
    }
    this.browserProxy_.providePIN(this.$.pin.value).then(retries => {
      this.confirmButtonDisabled_ = false;
      if (retries != null) {
        this.$.pin.showIncorrectPINError(retries);
        return;
      }
      this.showEnrollmentsPage_();
    });
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
        this.cancelButtonVisible_ = true;
        this.cancelButtonDisabled = false;
        this.confirmButtonVisible_ = false;
        this.doneButtonVisible_ = false;
        break;
      case 'pinPrompt':
        this.cancelButtonVisible_ = true;
        this.cancelButtonDisabled = false;
        this.confirmButtonVisible_ = true;
        this.confirmButtonDisabled_ = false;
        this.doneButtonVisible_ = false;
        break;
      case 'enrollments':
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = false;
        this.doneButtonVisible_ = true;
        break;
      case 'enroll':
        this.cancelButtonVisible_ = true;
        this.cancelButtonDisabled = false;
        this.confirmButtonVisible_ = false;
        this.doneButtonVisible_ = false;
        break;
      case 'chooseName':
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = true;
        this.confirmButtonDisabled_ = !this.recentEnrollmentName_.length;
        this.doneButtonVisible_ = false;
        this.$.enrollmentName.focus();
        break;
      case 'error':
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = false;
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

    this.recentEnrollmentId_ = '';
    this.recentEnrollmentName_ = '';

    this.dialogPage_ = 'enroll';

    this.browserProxy_.startEnrolling().then(response => {
      this.onEnrolling_(response);
    });
  },

  /**
   * @private
   * @param {!EnrollmentStatus} response
   */
  onEnrolling_: function(response) {
    if (response.code == Ctap2Status.ERR_KEEPALIVE_CANCEL) {
      this.showEnrollmentsPage_();
      return;
    }

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
      assert(response.enrollment);
      this.recentEnrollmentId_ = response.enrollment.id;
      this.recentEnrollmentName_ = response.enrollment.name;
      this.cancelButtonVisible_ = false;
      this.confirmButtonVisible_ = true;
      this.confirmButtonDisabled_ = false;
      this.$.confirmButton.focus();
    }
    this.fire('bio-enroll-dialog-ready-for-testing');
  },

  /** @private */
  confirmButtonClick_: function() {
    // Disable |confirmButton| while PIN verification or template enumeration is
    // pending. Resetting |dialogPage_| will re-enable it.
    this.confirmButtonDisabled_ = true;
    switch (this.dialogPage_) {
      case 'pinPrompt':
        this.submitPIN_();
        break;
      case 'enroll':
        assert(!!this.recentEnrollmentId_.length);
        this.dialogPage_ = 'chooseName';
        break;
      case 'chooseName':
        this.browserProxy_
            .renameEnrollment(
                this.recentEnrollmentId_, this.recentEnrollmentName_)
            .then(enrollments => {
              this.onEnrollments_(enrollments);
            });
        break;
      default:
        assertNotReached();
    }
  },

  /** @private */
  showEnrollmentsPage_: function() {
    this.browserProxy_.enumerateEnrollments().then(enrollments => {
      this.onEnrollments_(enrollments);
    });
  },

  /** @private */
  cancel_: function() {
    if (this.dialogPage_ == 'enroll') {
      // Cancel an ongoing enrollment.  Will cause the pending
      // enumerateEnrollments() promise to be resolved and proceed to the
      // enrollments page.
      this.cancelButtonDisabled_ = true;
      this.browserProxy_.cancelEnrollment();
    } else {
      // On any other screen, simply close the dialog.
      this.done_();
    }
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
  },

  /** @private */
  onEnrollmentNameInput_: function() {
    this.confirmButtonDisabled_ = !this.recentEnrollmentName_.length;
  },
});
})();
