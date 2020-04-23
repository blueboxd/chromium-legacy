// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-history-deletion-dialog' is a dialog that is
 * optionally shown inside settings-clear-browsing-data-dialog after deleting
 * browsing history. It informs the user about the existence of other forms
 * of browsing history in their account.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../settings_shared_css.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'settings-history-deletion-dialog',

  _template: html`{__html_template__}`,

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /**
   * Tap handler for the "OK" button.
   * @private
   */
  onOkTap_() {
    this.$.dialog.close();
  },
});
