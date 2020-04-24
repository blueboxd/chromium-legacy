// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'ca-trust-edit-dialog' allows the user to:
 *  - specify the trust level of a certificate authority that is being
 *    imported.
 *  - edit the trust level of an already existing certificate authority.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './certificate_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CaTrustInfo, CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode, NewCertificateSubNode} from './certificates_browser_proxy.js';

Polymer({
  is: 'ca-trust-edit-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CertificateSubnode|!NewCertificateSubNode} */
    model: Object,

    /** @private {?CaTrustInfo} */
    trustInfo_: Object,

    /** @private {string} */
    explanationText_: String,
  },

  /** @private {?CertificatesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = CertificatesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.explanationText_ = loadTimeData.getStringF(
        'certificateManagerCaTrustEditDialogExplanation', this.model.name);

    // A non existing |model.id| indicates that a new certificate is being
    // imported, otherwise an existing certificate is being edited.
    if (this.model.id) {
      this.browserProxy_.getCaCertificateTrust(this.model.id)
          .then(trustInfo => {
            this.trustInfo_ = trustInfo;
            this.$.dialog.showModal();
          });
    } else {
      /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
    }
  },

  /** @private */
  onCancelTap_() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  },

  /** @private */
  onOkTap_() {
    this.$.spinner.active = true;

    const whenDone = this.model.id ?
        this.browserProxy_.editCaCertificateTrust(
            this.model.id, this.$.ssl.checked, this.$.email.checked,
            this.$.objSign.checked) :
        this.browserProxy_.importCaCertificateTrustSelected(
            this.$.ssl.checked, this.$.email.checked, this.$.objSign.checked);

    whenDone.then(
        () => {
          this.$.spinner.active = false;
          /** @type {!CrDialogElement} */ (this.$.dialog).close();
        },
        error => {
          /** @type {!CrDialogElement} */ (this.$.dialog).close();
          this.fire('certificates-error', {error: error, anchor: null});
        });
  },
});
