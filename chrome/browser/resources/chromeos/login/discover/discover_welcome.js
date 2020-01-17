// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for the new Discover Welcome dialog.
 */

Polymer({
  is: 'discover-welcome',

  behaviors: [DiscoverModuleBehavior],

  properties: {
    /**
     * Overriding DiscoverModuleBehavior.moduleName.
     */
    moduleName: {
      type: String,
      readOnly: true,
      value: 'welcome',
    },
  },

  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  show() {
    // TODO(alemate)
  },

  /*
   * @private
   */
  onCloseButton_() {
    this.fire('module-skip');
  },

  /*
   * @private
   */
  onContinueButton_() {
    this.fire('module-continue');
  },
});
