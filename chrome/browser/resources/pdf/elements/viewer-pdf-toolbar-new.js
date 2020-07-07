// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import './icons.js';
import './viewer-download-controls.js';
import './viewer-page-selector.js';
import './shared-css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class ViewerPdfToolbarNewElement extends PolymerElement {
  static get is() {
    return 'viewer-pdf-toolbar-new';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      docTitle: String,
      docLength: Number,
      hasEdits: Boolean,
      hasEnteredAnnotationMode: Boolean,
      isFormFieldFocused: Boolean,

      loadProgress: {
        type: Number,
        observer: 'loadProgressChanged_',
      },

      loading_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      pageNo: Number,
      pdfAnnotationsEnabled: Boolean,
      pdfFormSaveEnabled: Boolean,
      printingEnabled: Boolean,
    };
  }

  constructor() {
    super();

    /** @private {boolean} */
    this.loading_ = true;
  }

  /** @private */
  loadProgressChanged_() {
    this.loading_ = this.loadProgress < 100;
  }

  /** @private */
  onPrintClick_() {
    this.dispatchEvent(new CustomEvent('print'));
  }
}

customElements.define(
    ViewerPdfToolbarNewElement.is, ViewerPdfToolbarNewElement);
