// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Recommend Apps
 * screen.
 */

Polymer({
  is: 'recommend-apps',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Reference to OOBE screen object.
     * @type {!{
     *     onInstall: function(),
     *     onRetry: function(),
     *     onSkip: function(),
     * }}
     */
    screen: {
      type: Object,
    },
  },

  focus() {
    this.getElement('recommend-apps-dialog').focus();
  },

  /** @private */
  onSkip_() {
    this.screen.onSkip();
  },

  /** @private */
  onInstall_() {
    this.screen.onInstall();
  },

  /**
   * Returns element by its id.
   * @param id String The ID of the element.
   */
  getElement(id) {
    return this.$[id];
  },
});
