// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'urgent-password-expiry-notification' is a notification that
 * warns the user their password is about to expire - but it is a large
 * notification that is shown in the center of the screen.
 * It is implemented not using the notification system, but as a system dialog.
 */

Polymer({
  is: 'urgent-password-expiry-notification',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /** @private */
  onButtonTap_: function() {
    chrome.send('continue');
  },

});
