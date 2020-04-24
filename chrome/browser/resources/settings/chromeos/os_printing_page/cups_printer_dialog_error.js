// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'printer-dialog-error' is the error container for dialogs.
 */
Polymer({
  is: 'printer-dialog-error',

  properties: {
    /** The error text to be displayed on the dialog. */
    errorText: {
      type: String,
      value: '',
    },
  },
});
