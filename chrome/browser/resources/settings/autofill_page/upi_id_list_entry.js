// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'upi-id-list-entry' is a UPI ID row to be shown in
 * the settings page. https://en.wikipedia.org/wiki/Unified_Payments_Interface
 */

Polymer({
  is: 'settings-upi-id-list-entry',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** A saved UPI ID. */
    upiId: String,
  },
});
