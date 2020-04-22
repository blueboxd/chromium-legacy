// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-edit-dialog' is the dialog that allows showing a
 *     saved password.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../icons.m.js';
import '../settings_shared_css.m.js';
import '../settings_vars_css.m.js';
import './passwords_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ShowPasswordBehavior} from './show_password_behavior.js';


Polymer({
  is: 'password-edit-dialog',

  _template: html`{__html_template__}`,

  behaviors: [ShowPasswordBehavior],

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  },

  /**
   * Handler for tapping the 'done' button. Should just dismiss the dialog.
   * @private
   */
  onActionButtonTap_() {
    this.close();
  },

  /** Manually de-select texts for readonly inputs. */
  onInputBlur_() {
    this.shadowRoot.getSelection().removeAllRanges();
  },
});
