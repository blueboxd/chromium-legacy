// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A confirmation dialog allowing the user to delete various types
 * of certificates.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import './certificate_shared_css.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode, CertificateType} from './certificates_browser_proxy.js';

Polymer({
  is: 'certificate-delete-confirmation-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CertificateSubnode} */
    model: Object,

    /** @type {!CertificateType} */
    certificateType: String,
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

  /**
   * @private
   * @return {string}
   */
  getTitleText_() {
    /**
     * @param {string} localizedMessageId
     * @return {string}
     */
    const getString = localizedMessageId =>
        loadTimeData.getStringF(localizedMessageId, this.model.name);

    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return getString('certificateManagerDeleteUserTitle');
      case CertificateType.SERVER:
        return getString('certificateManagerDeleteServerTitle');
      case CertificateType.CA:
        return getString('certificateManagerDeleteCaTitle');
      case CertificateType.OTHER:
        return getString('certificateManagerDeleteOtherTitle');
    }
    assertNotReached();
  },

  /**
   * @private
   * @return {string}
   */
  getDescriptionText_() {
    const getString = loadTimeData.getString.bind(loadTimeData);
    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return getString('certificateManagerDeleteUserDescription');
      case CertificateType.SERVER:
        return getString('certificateManagerDeleteServerDescription');
      case CertificateType.CA:
        return getString('certificateManagerDeleteCaDescription');
      case CertificateType.OTHER:
        return '';
    }
    assertNotReached();
  },

  /** @private */
  onCancelTap_() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  },

  /** @private */
  onOkTap_() {
    this.browserProxy_.deleteCertificate(this.model.id)
        .then(
            () => {
              /** @type {!CrDialogElement} */ (this.$.dialog).close();
            },
            error => {
              /** @type {!CrDialogElement} */ (this.$.dialog).close();
              this.fire('certificates-error', {error: error, anchor: null});
            });
  },
});
