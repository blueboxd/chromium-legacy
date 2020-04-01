// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'signin-error-app',

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private {boolean} */
    isSystemProfile_: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('isSystemProfile'),
    },

    /** @private {boolean} */
    switchButtonUnavailable_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    hideNormalError_: {
      type: Boolean,
      value: () => loadTimeData.getString('signinErrorMessage').length === 0,
    },

    /**
     * An array of booleans indicating whether profile blocking messages should
     * be hidden. Position 0 corresponds to the #profile-blocking-error-message
     * container, and subsequent positions correspond to each of the 3 related
     * messages respectively.
     * @private {!Array<boolean>}
     */
    hideProfileBlockingErrors_: {
      type: Array,
      value: function() {
        const hide = [
          'profileBlockedMessage',
          'profileBlockedAddPersonSuggestion',
          'profileBlockedRemoveProfileSuggestion',
        ].map(id => loadTimeData.getString(id).length === 0);

        // Hide the container itself if all of each children are also hidden.
        hide.unshift(hide.every(hideEntry => hideEntry));

        return hide;
      },
    },
  },

  /** @private {?function(!Event)} */
  boundKeyDownHandler_: null,

  /** @override */
  attached() {
    this.boundKeyDownHandler_ = this.onKeyDown_.bind(this);
    document.addEventListener('keydown', this.boundKeyDownHandler_);

    this.addWebUIListener('switch-button-unavailable', () => {
      this.switchButtonUnavailable_ = true;
    });
  },

  /** @override */
  detached() {
    document.removeEventListener('keydown', this.boundKeyDownHandler_);
    this.boundKeyDownHandler_ = null;
  },

  /** @private */
  onConfirm_() {
    chrome.send('confirm');
  },

  /** @private */
  onSwitchToExistingProfile_() {
    chrome.send('switchToExistingProfile');
  },

  /** @private */
  onLearnMore_() {
    chrome.send('learnMore');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKeyDown_(e) {
    if (e.key == 'Enter' &&
        !/^(A|CR-BUTTON)$/.test(e.composedPath()[0].tagName)) {
      this.onConfirm_();
      e.preventDefault();
    }
  },
});
