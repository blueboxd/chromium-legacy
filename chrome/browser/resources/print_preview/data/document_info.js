// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * @typedef {{
 *   hasCssMediaStyles: boolean,
 *   hasSelection: boolean,
 *   isModifiable: boolean,
 *   isScalingDisabled: boolean,
 *   fitToPageScaling: number,
 *   pageCount: number,
 *   title: string,
 * }}
 */
print_preview.DocumentSettings;

/**
 * @typedef {{
 *   marginTop: number,
 *   marginLeft: number,
 *   marginBottom: number,
 *   marginRight: number,
 *   contentWidth: number,
 *   contentHeight: number,
 *   printableAreaX: number,
 *   printableAreaY: number,
 *   printableAreaWidth: number,
 *   printableAreaHeight: number,
 * }}
 */
print_preview.PageLayoutInfo;

Polymer({
  is: 'print-preview-document-info',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {!print_preview.DocumentSettings} */
    documentSettings: {
      type: Object,
      notify: true,
      value: function() {
        return {
          hasCssMediaStyles: false,
          hasSelection: false,
          isModifiable: true,
          isScalingDisabled: false,
          fitToPageScaling: 100,
          pageCount: 0,
          title: '',
        };
      },
    },

    inFlightRequestId: {
      type: Number,
      value: -1,
    },

    /** @type {print_preview.Margins} */
    margins: {
      type: Object,
      notify: true,
    },

    /**
     * Size of the pages of the document in points. Actual page-related
     * information won't be set until preview generation occurs, so use
     * a default value until then.
     * @type {!print_preview.Size}
     */
    pageSize: {
      type: Object,
      notify: true,
      value: function() {
        return new print_preview.Size(612, 792);
      },
    },

    /**
     * Printable area of the document in points.
     * @type {!print_preview.PrintableArea}
     */
    printableArea: {
      type: Object,
      notify: true,
      value: function() {
        return new print_preview.PrintableArea(
            new print_preview.Coordinate2d(0, 0),
            new print_preview.Size(612, 792));
      },
    },
  },

  /**
   * Whether this data model has been initialized.
   * @private {boolean}
   */
  isInitialized_: false,

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'page-count-ready', this.onPageCountReady_.bind(this));
    this.addWebUIListener(
        'page-layout-ready', this.onPageLayoutReady_.bind(this));
  },

  /**
   * Initializes the state of the data model.
   * @param {boolean} isModifiable Whether the document is modifiable.
   * @param {string} title Title of the document.
   * @param {boolean} hasSelection Whether the document has user-selected
   *     content.
   */
  init: function(isModifiable, title, hasSelection) {
    this.isInitialized_ = true;
    this.set('documentSettings.isModifiable', isModifiable);
    this.set('documentSettings.title', title);
    this.set('documentSettings.hasSelection', hasSelection);
  },

  /**
   * Updates whether scaling is disabled for the document.
   * @param {boolean} isScalingDisabled Whether scaling of the document is
   *     prohibited.
   */
  updateIsScalingDisabled: function(isScalingDisabled) {
    if (this.isInitialized_) {
      this.set('documentSettings.isScalingDisabled', isScalingDisabled);
    }
  },

  /**
   * Called when the page layout of the document is ready. Always occurs
   * as a result of a preview request.
   * @param {!print_preview.PageLayoutInfo} pageLayout Layout information
   *     about the document.
   * @param {boolean} hasCustomPageSizeStyle Whether this document has a
   *     custom page size or style to use.
   * @private
   */
  onPageLayoutReady_: function(pageLayout, hasCustomPageSizeStyle) {
    const origin = new print_preview.Coordinate2d(
        pageLayout.printableAreaX, pageLayout.printableAreaY);
    const size = new print_preview.Size(
        pageLayout.printableAreaWidth, pageLayout.printableAreaHeight);

    const margins = new print_preview.Margins(
        Math.round(pageLayout.marginTop), Math.round(pageLayout.marginRight),
        Math.round(pageLayout.marginBottom), Math.round(pageLayout.marginLeft));

    const o = print_preview.ticket_items.CustomMarginsOrientation;
    const pageSize = new print_preview.Size(
        pageLayout.contentWidth + margins.get(o.LEFT) + margins.get(o.RIGHT),
        pageLayout.contentHeight + margins.get(o.TOP) + margins.get(o.BOTTOM));

    if (this.isInitialized_) {
      this.printableArea = new print_preview.PrintableArea(origin, size);
      this.pageSize = pageSize;
      this.set('documentSettings.hasCssMediaStyles', hasCustomPageSizeStyle);
      this.margins = margins;
    }
  },

  /**
   * Called when the document page count is received from the native layer.
   * Always occurs as a result of a preview request.
   * @param {number} pageCount The document's page count.
   * @param {number} previewResponseId The request ID for this page count event.
   * @param {number} fitToPageScaling The scaling required to fit the document
   *     to page.
   * @private
   */
  onPageCountReady_: function(pageCount, previewResponseId, fitToPageScaling) {
    if (this.inFlightRequestId != previewResponseId || !this.isInitialized_) {
      return;
    }
    this.set('documentSettings.pageCount', pageCount);
    this.set('documentSettings.fitToPageScaling', fitToPageScaling);
  },
});
