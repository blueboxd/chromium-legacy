// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog for showing SSL certificate related error messages.
 * The user can only close the dialog, there is no other possible interaction.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import './certificate_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificatesError, CertificatesImportError} from './certificates_browser_proxy.js';

Polymer({
  is: 'certificates-error-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CertificatesError|!CertificatesImportError} */
    model: Object,
  },

  /** @override */
  attached() {
    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  },

  /** @private */
  onOkTap_() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  },

  /**
   * @param {{name: string, error: string}} importError
   * @return {string}
   * @private
   */
  getCertificateErrorText_(importError) {
    return loadTimeData.getStringF(
        'certificateImportErrorFormat', importError.name, importError.error);
  },
});
