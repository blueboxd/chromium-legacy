// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-page' is the settings page containing the browser
 * safety check.
 */
(function() {

/**
 * States of the safety check parent element.
 * @enum {number}
 */
const ParentStatus = {
  BEFORE: 0,
  CHECKING: 1,
  AFTER: 2,
};

Polymer({
  is: 'settings-safety-check-page',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check parent element.
     * @private {!ParentStatus}
     */
    parentStatus_: {
      type: Number,
      value: ParentStatus.BEFORE,
    },
  },

  /**
   * Triggers the safety check.
   * @private
   */
  onRunSafetyCheckClick_: function() {
    // TODO(crbug.com/1010001) Trigger backend safety check here.
    this.parentStatus_ = ParentStatus.CHECKING;
  },

  /**
   * @private
   * @return {boolean}
   */
  showParentButton_: function() {
    return this.parentStatus_ == ParentStatus.BEFORE;
  },

  /**
   * @private
   * @return {boolean}
   */
  showParentIconButton_: function() {
    return this.parentStatus_ == ParentStatus.AFTER;
  },

  /**
   * @private
   * @return {string}
   */
  getParentPrimaryLabelText_: function() {
    switch (this.parentStatus_) {
      case ParentStatus.BEFORE:
        return this.i18n('safetyCheckParentPrimaryLabelBefore');
      case ParentStatus.CHECKING:
        return this.i18n('safetyCheckParentPrimaryLabelChecking');
      case ParentStatus.AFTER:
        return this.i18n('safetyCheckParentPrimaryLabelAfter');
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getParentIcon_: function() {
    switch (this.parentStatus_) {
      case ParentStatus.BEFORE:
      case ParentStatus.AFTER:
        return 'settings:assignment';
      case ParentStatus.CHECKING:
        return null;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getParentIconSrc_: function() {
    switch (this.parentStatus_) {
      case ParentStatus.BEFORE:
      case ParentStatus.AFTER:
        return null;
      case ParentStatus.CHECKING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {string}
   */
  getParentIconClass_: function() {
    switch (this.parentStatus_) {
      case ParentStatus.BEFORE:
      case ParentStatus.CHECKING:
        return 'icon-blue';
      case ParentStatus.AFTER:
        return '';
      default:
        assertNotReached();
    }
  },
});
})();
