// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** 'add-printer-dialog' is the template of the Add Printer dialog. */
Polymer({
  is: 'add-printer-dialog',

  /** @private */
  attached() {
    this.$.dialog.showModal();
  },

  close() {
    this.$.dialog.close();
  },
});
