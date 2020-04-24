// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog prompting the user to encrypt a personal certificate
 * before it is exported to disk.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './certificate_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode} from './certificates_browser_proxy.js';

Polymer({
  is: 'certificate-password-encryption-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CertificateSubnode} */
    model: Object,

    /** @private */
    password_: {
      type: String,
      value: '',
    },

    /** @private */
    confirmPassword_: {
      type: String,
      value: '',
    },
  },

  /** @private {?CertificatesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = CertificatesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  },

  /** @private */
  onCancelTap_() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  },

  /** @private */
  onOkTap_() {
    this.browserProxy_.exportPersonalCertificatePasswordSelected(this.password_)
        .then(
            () => {
              this.$.dialog.close();
            },
            error => {
              this.$.dialog.close();
              this.fire('certificates-error', {error: error, anchor: null});
            });
  },

  /** @private */
  validate_() {
    const isValid =
        this.password_ !== '' && this.password_ === this.confirmPassword_;
    this.$.ok.disabled = !isValid;
  },
});
