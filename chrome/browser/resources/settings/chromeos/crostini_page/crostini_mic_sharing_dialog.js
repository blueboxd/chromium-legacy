// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-mic-sharing-dialog' is a component that
 * alerts the user when Crostini needs to be restarted in order for changes to
 * the mic sharing settings to take effect.
 */
Polymer({
  is: 'settings-crostini-mic-sharing-dialog',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * An attribute that indicates what CrostiniMicSharingEnabled should be set
     * to, if Crostini is shutdown.
     */
    pendingMicSharingState: {
      type: Boolean,
    },
  },
  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onShutdownTap_() {
    // The mic sharing value is read only when Crostini starts up, so update
    // this value before shutting Crostini down to ensure that the updated value
    // will be available before Crostini reads it.
    settings.CrostiniBrowserProxyImpl.getInstance()
        .setCrostiniMicSharingEnabled(this.pendingMicSharingState);
    settings.CrostiniBrowserProxyImpl.getInstance().shutdownCrostini();
    this.$.dialog.close();
  },
});
