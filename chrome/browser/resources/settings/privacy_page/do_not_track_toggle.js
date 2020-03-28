// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-do-not-track-toggle',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onDomChange_() {
    if (this.showDialog_) {
      this.$$('#confirmDialog').showModal();
    }
  },

  /**
   * Handles the change event for the do-not-track toggle. Shows a
   * confirmation dialog when enabling the setting.
   * @param {!Event} event
   * @private
   */
  onToggleChange_(event) {
    settings.MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        settings.PrivacyElementInteractions.DO_NOT_TRACK);
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    if (!target.checked) {
      // Always allow disabling the pref.
      target.sendPrefChange();
      return;
    }

    this.showDialog_ = true;
  },

  /** @private */
  closeDialog_() {
    this.$$('#confirmDialog').close();
    this.showDialog_ = false;
  },

  /** @private */
  onDialogClosed_() {
    cr.ui.focusWithoutInk(this.$.toggle);
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onDialogConfirm_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.toggle)
        .sendPrefChange();
    this.closeDialog_();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onDialogCancel_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.toggle)
        .resetToPrefValue();
    this.closeDialog_();
  },
});
