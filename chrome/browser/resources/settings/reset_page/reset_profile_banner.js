// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-profile-banner' is the banner shown for prompting the user to
 * clear profile settings.
 */
Polymer({
  // TODO(dpapad): Rename to settings-reset-warning-dialog.
  is: 'settings-reset-profile-banner',

  listeners: {
    'cancel': 'onCancel_',
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onOkTap_() {
    this.$.dialog.cancel();
  },

  /** @private */
  onCancel_() {
    settings.ResetBrowserProxyImpl.getInstance().onHideResetProfileBanner();
  },

  /** @private */
  onResetTap_() {
    this.$.dialog.close();
    settings.Router.getInstance().navigateTo(settings.routes.RESET_DIALOG);
  },
});
