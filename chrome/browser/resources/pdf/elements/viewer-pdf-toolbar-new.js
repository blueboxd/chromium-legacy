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

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FittingType} from '../constants.js';

export class ViewerPdfToolbarNewElement extends PolymerElement {
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

      fittingType_: Number,

      /** @private {string} */
      fitToButtonIcon_: {
        type: String,
        computed: 'computeFitToButtonIcon_(fittingType_)',
      },
    };
  }

  constructor() {
    super();

    /** @private {!FittingType} */
    this.fittingType_ = FittingType.FIT_TO_PAGE;

    /** @private {boolean} */
    this.loading_ = true;
  }

  /**
   * @return {string}
   * @private
   */
  computeFitToButtonIcon_() {
    return this.fittingType_ === FittingType.FIT_TO_PAGE ? 'pdf:fit-to-height' :
                                                           'pdf:fit-to-width';
  }

  /**
   * @param {string} fitToPageTooltip
   * @param {string} fitToWidthTooltip
   * @return {string} The appropriate tooltip for the current state
   * @private
   */
  getFitToButtonTooltip_(fitToPageTooltip, fitToWidthTooltip) {
    return this.fittingType_ === FittingType.FIT_TO_PAGE ? fitToPageTooltip :
                                                           fitToWidthTooltip;
  }

  /** @private */
  loadProgressChanged_() {
    this.loading_ = this.loadProgress < 100;
  }

  /** @private */
  onPrintClick_() {
    this.dispatchEvent(new CustomEvent('print'));
  }

  /** @private */
  onZoomInClick_() {
    this.dispatchEvent(new CustomEvent('zoom-in'));
  }

  /** @private */
  onZoomOutClick_() {
    this.dispatchEvent(new CustomEvent('zoom-out'));
  }

  /** @param {!FittingType} fittingType */
  forceFit(fittingType) {
    this.fittingType_ = fittingType;
  }

  fitToggle() {
    const newState = this.fittingType_ === FittingType.FIT_TO_PAGE ?
        FittingType.FIT_TO_WIDTH :
        FittingType.FIT_TO_PAGE;
    this.dispatchEvent(
        new CustomEvent('fit-to-changed', {detail: this.fittingType_}));
    this.fittingType_ = newState;
  }

  /** @private */
  onFitToButtonClick_() {
    this.fitToggle();
  }

  /** @private */
  onMoreClick_() {
    const menu = this.shadowRoot.querySelector('cr-action-menu');
    menu.showAt(this.shadowRoot.querySelector('#more'), {
      anchorAlignmentX: AnchorAlignment.CENTER,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }
}

customElements.define(
    ViewerPdfToolbarNewElement.is, ViewerPdfToolbarNewElement);
