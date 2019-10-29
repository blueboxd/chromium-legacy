// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'signin-email-confirmation-app',

  /** @override */
  ready: function() {
    const args = /** @type {{lastEmail: string, newEmail: string}} */
        (JSON.parse(chrome.getVariableValue('dialogArguments')));
    const {lastEmail, newEmail} = args;
    this.$.dialogTitle.textContent =
        loadTimeData.getStringF('signinEmailConfirmationTitle', lastEmail);
    this.$.createNewUserRadioButtonSubtitle.textContent =
        loadTimeData.getStringF(
            'signinEmailConfirmationCreateProfileButtonSubtitle', newEmail);
    this.$.startSyncRadioButtonSubtitle.textContent = loadTimeData.getStringF(
        'signinEmailConfirmationStartSyncButtonSubtitle', newEmail);

    document.addEventListener('keydown', this.onKeyDown_.bind(this));
  },

  onKeyDown_: function(e) {
    // If the currently focused element isn't something that performs an action
    // on "enter" being pressed and the user hits "enter", perform the default
    // action of the dialog, which is "OK".
    if (e.key == 'Enter' &&
        !/^(A|CR-BUTTON)$/.test(getDeepActiveElement().tagName)) {
      this.$.confirmButton.click();
      e.preventDefault();
    }
  },

  /** @private */
  onConfirm_: function() {
    const action = this.$$('cr-radio-group').selected;
    chrome.send('dialogClose', [JSON.stringify({'action': action})]);
  },

  /** @private */
  onCancel_: function() {
    chrome.send('dialogClose', [JSON.stringify({'action': 'cancel'})]);
  },
});
