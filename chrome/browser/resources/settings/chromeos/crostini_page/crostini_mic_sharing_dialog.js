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

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onOkTap_() {
    this.$.dialog.close();
  },
});
