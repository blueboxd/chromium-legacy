// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-cellular-setup-dialog' embeds the <cellular-setup>
 * that is shared with OOBE in a dialog with OS Settings stylizations.
 */
Polymer({
  is: 'os-settings-cellular-setup-dialog',

  properties: {},

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },
});
