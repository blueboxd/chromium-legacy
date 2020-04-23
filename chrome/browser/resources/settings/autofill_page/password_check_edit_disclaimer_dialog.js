// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'settings-password-edit-disclaimer-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The website origin that is being displayed.
     */
    origin: String,
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onEditClick_() {
    this.fire('edit-password-click');
    this.$.dialog.close();
  },

  /** @private */
  onCancel_() {
    this.$.dialog.close();
  },

  /**
   * @return {string}
   * @private
   */
  getDisclaimerTitle_() {
    return this.i18n('editDisclaimerTitle', this.origin);
  },
});
