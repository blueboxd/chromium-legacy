// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * @typedef {{
 *   width_microns: number,
 *   height_microns: number,
 * }}
 */
print_preview.MediaSizeValue;

/** @enum {string} */
print_preview.PreviewAreaState = {
  NO_PLUGIN: 'no-plugin',
  LOADING: 'loading',
  DISPLAY_PREVIEW: 'display-preview',
  OPEN_IN_PREVIEW_LOADING: 'open-in-preview-loading',
  OPEN_IN_PREVIEW_LOADED: 'open-in-preview-loaded',
  ERROR: 'error',
};

Polymer({
  is: 'print-preview-preview-area',

  behaviors: [
    WebUIListenerBehavior,
    SettingsBehavior,
    I18nBehavior,
    print_preview.DarkModeBehavior,
  ],

  properties: {
    /** @type {print_preview.Destination} */
    destination: Object,

    documentModifiable: Boolean,

    /** @type {!print_preview.Error} */
    error: {
      type: Number,
      notify: true,
    },

    /** @type {print_preview.Margins} */
    margins: Object,

    /** @type {?print_preview.MeasurementSystem} */
    measurementSystem: Object,

    newPrintPreviewLayout: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {!print_preview.Size} */
    pageSize: Object,

    /** @type {!print_preview.PreviewAreaState} */
    previewState: {
      type: String,
      notify: true,
      value: print_preview.PreviewAreaState.LOADING,
    },

    /** @type {!print_preview.State} */
    state: Number,

    /** @private {boolean} Whether the plugin is loaded */
    pluginLoaded_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} Whether the document is ready */
    documentReady_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    previewLoaded_: {
      type: Boolean,
      notify: true,
      computed: 'computePreviewLoaded_(documentReady_, pluginLoaded_)',
    },
  },

  listeners: {
    'pointerover': 'onPointerOver_',
    'pointerout': 'onPointerOut_',
  },

  observers: [
    'onDarkModeChanged_(inDarkMode)',
    'pluginOrDocumentStatusChanged_(pluginLoaded_, documentReady_)',
    'onStateOrErrorChange_(state, error)',
  ],

  /** @private {?print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @private {?Object} */
  lastTicket_: null,

  /** @private {number} */
  inFlightRequestId_: -1,

  /** @private {?print_preview.PluginProxy} */
  pluginProxy_: null,

  /** @private {?function(!KeyboardEvent)} */
  keyEventCallback_: null,

  /** @override */
  attached: function() {
    this.nativeLayer_ = print_preview.NativeLayer.getInstance();
    this.addWebUIListener(
        'page-preview-ready', this.onPagePreviewReady_.bind(this));

    if (!this.pluginProxy_.checkPluginCompatibility(assert(
            this.$$('.preview-area-compatibility-object-out-of-process')))) {
      this.error = print_preview.Error.NO_PLUGIN;
      this.previewState = print_preview.PreviewAreaState.ERROR;
    }
  },

  /** @override */
  created: function() {
    this.pluginProxy_ = print_preview.PluginProxy.getInstance();
  },

  /**
   * @return {boolean} Whether the preview is loaded.
   * @private
   */
  computePreviewLoaded_: function() {
    return this.documentReady_ && this.pluginLoaded_;
  },

  /** @return {boolean} Whether the preview is loaded. */
  previewLoaded: function() {
    return this.previewLoaded_;
  },

  /**
   * Called when the pointer moves onto the component. Shows the margin
   * controls if custom margins are being used.
   * @param {!Event} event Contains element pointer moved from.
   * @private
   */
  onPointerOver_: function(event) {
    const marginControlContainer = this.$.marginControlContainer;
    let fromElement = event.fromElement;
    while (fromElement != null) {
      if (fromElement == marginControlContainer) {
        return;
      }

      fromElement = fromElement.parentElement;
    }
    marginControlContainer.setInvisible(false);
  },

  /**
   * Called when the pointer moves off of the component. Hides the margin
   * controls if they are visible.
   * @param {!Event} event Contains element pointer moved to.
   * @private
   */
  onPointerOut_: function(event) {
    const marginControlContainer = this.$.marginControlContainer;
    let toElement = event.toElement;
    while (toElement != null) {
      if (toElement == marginControlContainer) {
        return;
      }

      toElement = toElement.parentElement;
    }
    marginControlContainer.setInvisible(true);
  },

  /** @private */
  pluginOrDocumentStatusChanged_: function() {
    if (!this.pluginLoaded_ || !this.documentReady_ ||
        this.previewState === print_preview.PreviewAreaState.ERROR) {
      return;
    }

    this.previewState = this.previewState ==
            print_preview.PreviewAreaState.OPEN_IN_PREVIEW_LOADING ?
        print_preview.PreviewAreaState.OPEN_IN_PREVIEW_LOADED :
        print_preview.PreviewAreaState.DISPLAY_PREVIEW;
  },

  /**
   * @return {string} 'invisible' if overlay is invisible, '' otherwise.
   * @private
   */
  getInvisible_: function() {
    return this.isInDisplayPreviewState_() ? 'invisible' : '';
  },

  /**
   * @return {string} 'true' if overlay is aria-hidden, 'false' otherwise.
   * @private
   */
  getAriaHidden_: function() {
    return this.isInDisplayPreviewState_().toString();
  },

  /**
   * @return {boolean} Whether the preview area is in DISPLAY_PREVIEW state.
   * @private
   */
  isInDisplayPreviewState_: function() {
    return this.previewState == print_preview.PreviewAreaState.DISPLAY_PREVIEW;
  },

  /**
   * @return {boolean} Whether the preview is currently loading.
   * @private
   */
  isPreviewLoading_: function() {
    return this.previewState == print_preview.PreviewAreaState.LOADING;
  },

  /**
   * @return {string} 'jumping-dots' to enable animation, '' otherwise.
   * @private
   */
  getJumpingDots_: function() {
    return this.isPreviewLoading_() ? 'jumping-dots' : '';
  },

  /**
   * @return {boolean} Whether the "learn more" link to the cloud print help
   *     page should be shown.
   * @private
   */
  shouldShowLearnMoreLink_: function() {
    return this.error === print_preview.Error.UNSUPPORTED_PRINTER;
  },

  /**
   * @return {string} The current preview area message to display.
   * @private
   */
  currentMessage_: function() {
    switch (this.previewState) {
      case print_preview.PreviewAreaState.LOADING:
        return this.i18n('loading');
      case print_preview.PreviewAreaState.DISPLAY_PREVIEW:
        return '';
      // <if expr="is_macosx">
      case print_preview.PreviewAreaState.OPEN_IN_PREVIEW_LOADING:
      case print_preview.PreviewAreaState.OPEN_IN_PREVIEW_LOADED:
        return this.i18n('openingPDFInPreview');
      // </if>
      case print_preview.PreviewAreaState.ERROR:
        // The preview area is responsible for displaying all errors except
        // print failed and cloud print error.
        return this.getErrorMessage_();
      default:
        return '';
    }
  },

  /**
   * @param {boolean} forceUpdate Whether to force the preview area to update
   *     regardless of whether the print ticket has changed.
   */
  startPreview: function(forceUpdate) {
    if (!this.hasTicketChanged_() && !forceUpdate &&
        this.previewState !== print_preview.PreviewAreaState.ERROR) {
      return;
    }
    this.previewState = print_preview.PreviewAreaState.LOADING;
    this.documentReady_ = false;
    this.getPreview_().then(
        previewUid => {
          if (!this.documentModifiable) {
            this.onPreviewStart_(previewUid, -1);
          }
          this.documentReady_ = true;
        },
        type => {
          if (/** @type{string} */ (type) == 'SETTINGS_INVALID') {
            this.error = print_preview.Error.INVALID_PRINTER;
            this.previewState = print_preview.PreviewAreaState.ERROR;
          } else if (/** @type{string} */ (type) != 'CANCELLED') {
            this.error = print_preview.Error.PREVIEW_FAILED;
            this.previewState = print_preview.PreviewAreaState.ERROR;
          }
        });
  },

  // <if expr="is_macosx">
  /** Set the preview state to display the "opening in preview" message. */
  setOpeningPdfInPreview: function() {
    assert(cr.isMac);
    this.previewState =
        this.previewState == print_preview.PreviewAreaState.LOADING ?
        print_preview.PreviewAreaState.OPEN_IN_PREVIEW_LOADING :
        print_preview.PreviewAreaState.OPEN_IN_PREVIEW_LOADED;
  },
  // </if>

  /**
   * @param {number} previewUid The unique identifier of the preview.
   * @param {number} index The index of the page to preview.
   * @private
   */
  onPreviewStart_: function(previewUid, index) {
    if (!this.pluginProxy_.pluginReady()) {
      const plugin = this.pluginProxy_.createPlugin(previewUid, index);
      this.pluginProxy_.setKeyEventCallback(this.keyEventCallback_);
      this.$$('.preview-area-plugin-wrapper')
          .appendChild(
              /** @type {Node} */ (plugin));
      this.pluginProxy_.setLoadCallback(this.onPluginLoad_.bind(this));
      this.pluginProxy_.setViewportChangedCallback(
          this.onPreviewVisualStateChange_.bind(this));
    }

    this.pluginLoaded_ = false;
    if (this.inDarkMode && this.newPrintPreviewLayout) {
      this.pluginProxy_.darkModeChanged(true);
    }
    this.pluginProxy_.resetPrintPreviewMode(
        previewUid, index, !this.getSettingValue('color'),
        /** @type {!Array<number>} */ (this.getSettingValue('pages')),
        this.documentModifiable);
  },

  /**
   * Called when the plugin loads. This is a consequence of calling
   * plugin.reload(). Certain plugin state can only be set after the plugin
   * has loaded.
   * @param {boolean} success Whether the plugin load succeeded or not.
   * @private
   */
  onPluginLoad_: function(success) {
    if (success) {
      this.pluginLoaded_ = true;
    } else {
      this.error = print_preview.Error.PREVIEW_FAILED;
      this.previewState = print_preview.PreviewAreaState.ERROR;
    }
  },

  /**
   * Called when the preview plugin's visual state has changed. This is a
   * consequence of scrolling or zooming the plugin. Updates the custom
   * margins component if shown.
   * @param {number} pageX The horizontal offset for the page corner in pixels.
   * @param {number} pageY The vertical offset for the page corner in pixels.
   * @param {number} pageWidth The page width in pixels.
   * @param {number} viewportWidth The viewport width in pixels.
   * @param {number} viewportHeight The viewport height in pixels.
   * @private
   */
  onPreviewVisualStateChange_: function(
      pageX, pageY, pageWidth, viewportWidth, viewportHeight) {
    // Ensure the PDF viewer isn't tabbable if the window is small enough that
    // the zoom toolbar isn't displayed.
    const tabindex = viewportWidth < 300 || viewportHeight < 200 ? '-1' : '0';
    this.$$('.preview-area-plugin').setAttribute('tabindex', tabindex);
    this.$.marginControlContainer.updateTranslationTransform(
        new print_preview.Coordinate2d(pageX, pageY));
    this.$.marginControlContainer.updateScaleTransform(
        pageWidth / this.pageSize.width);
    this.$.marginControlContainer.updateClippingMask(
        new print_preview.Size(viewportWidth, viewportHeight));
  },

  /**
   * Called when a page's preview has been generated.
   * @param {number} pageIndex The index of the page whose preview is ready.
   * @param {number} previewUid The unique ID of the print preview UI.
   * @param {number} previewResponseId The preview request ID that this page
   *     preview is a response to.
   * @private
   */
  onPagePreviewReady_: function(pageIndex, previewUid, previewResponseId) {
    if (this.inFlightRequestId_ != previewResponseId) {
      return;
    }
    const pageNumber = pageIndex + 1;
    let index = this.getSettingValue('pages').indexOf(pageNumber);
    // When pagesPerSheet > 1, the backend will always return page indices 0 to
    // N-1, where N is the total page count of the N-upped document.
    const pagesPerSheet =
        /** @type {number} */ (this.getSettingValue('pagesPerSheet'));
    if (pagesPerSheet > 1) {
      index = pageIndex;
    }
    if (index == 0) {
      this.onPreviewStart_(previewUid, pageIndex);
    }
    if (index != -1) {
      this.pluginProxy_.loadPreviewPage(previewUid, pageIndex, index);
    }
  },

  /** @private */
  onDarkModeChanged_: function() {
    if (!this.newPrintPreviewLayout) {
      return;
    }

    if (this.pluginProxy_.pluginReady()) {
      this.pluginProxy_.darkModeChanged(this.inDarkMode);
    }

    if (this.previewState === print_preview.PreviewAreaState.DISPLAY_PREVIEW) {
      this.startPreview(true);
    }
  },

  /**
   * Processes a keyboard event that could possibly be used to change state of
   * the preview plugin.
   * @param {!KeyboardEvent} e Keyboard event to process.
   */
  handleDirectionalKeyEvent: function(e) {
    // Make sure the PDF plugin is there.
    // We only care about: PageUp, PageDown, Left, Up, Right, Down.
    // If the user is holding a modifier key, ignore.
    if (!this.pluginProxy_.pluginReady() ||
        !['PageUp', 'PageDown', 'ArrowLeft', 'ArrowRight', 'ArrowUp',
          'ArrowDown']
             .includes(e.code) ||
        hasKeyModifiers(e)) {
      return;
    }

    // Don't handle the key event for these elements.
    const tagName = e.composedPath()[0].tagName;
    if (['INPUT', 'SELECT', 'EMBED'].includes(tagName)) {
      return;
    }

    // For the most part, if any div of header was the last clicked element,
    // then the active element is the body. Starting with the last clicked
    // element, and work up the DOM tree to see if any element has a
    // scrollbar. If there exists a scrollbar, do not handle the key event
    // here.
    const isEventHorizontal = ['ArrowLeft', 'ArrowRight'].includes(e.code);
    for (let i = 0; i < e.composedPath().length; i++) {
      const element = e.composedPath()[i];
      if (element.scrollHeight > element.clientHeight && !isEventHorizontal ||
          element.scrollWidth > element.clientWidth && isEventHorizontal) {
        return;
      }
    }

    // No scroll bar anywhere, or the active element is something else, like a
    // button. Note: buttons have a bigger scrollHeight than clientHeight.
    this.pluginProxy_.sendKeyEvent(e);
    e.preventDefault();
  },

  /**
   * Sends a message to the plugin to hide the toolbars after a delay.
   */
  hideToolbars: function() {
    if (!this.pluginProxy_.pluginReady()) {
      return;
    }

    this.pluginProxy_.hideToolbars();
  },

  /**
   * Set a callback that gets called when a key event is received that
   * originates in the plugin.
   * @param {function(KeyboardEvent)} callback The callback to be called with
   *     a key event.
   */
  setPluginKeyEventCallback: function(callback) {
    this.keyEventCallback_ = callback;
  },

  /**
   * Called when dragging margins starts or stops.
   */
  onMarginDragChanged_: function(e) {
    if (!this.pluginProxy_.pluginReady()) {
      return;
    }

    // When hovering over the plugin (which may be in a separate iframe)
    // pointer events will be sent to the frame. When dragging the margins,
    // we don't want this to happen as it can cause the margin to stop
    // being draggable.
    this.pluginProxy_.setPointerEvents(!e.detail);
  },

  /**
   * @param {!CustomEvent<{x: number, y: number}>} e Contains information about
   *     where the plugin should scroll to.
   * @private
   */
  onTextFocusPosition_: function(e) {
    // TODO(tkent): This is a workaround of a preview-area scrolling
    // issue. Blink scrolls preview-area on focus, but we don't want it.  We
    // should adjust scroll position of PDF preview and positions of
    // MarginContgrols here, or restructure the HTML so that the PDF review
    // and MarginControls are on the single scrollable container.
    // crbug.com/601341
    this.scrollTop = 0;
    this.scrollLeft = 0;

    const position = e.detail;
    if (position.x === 0 && position.y === 0) {
      return;
    }

    this.pluginProxy_.scrollPosition(position.x, position.y);
  },

  /**
   * @return {boolean} Whether margin settings are valid for the print ticket.
   * @private
   */
  marginsValid_: function() {
    const type = this.getSettingValue('margins');
    if (!Object.values(print_preview.ticket_items.MarginsTypeValue)
             .includes(type)) {
      // Unrecognized margins type.
      return false;
    }

    if (type !== print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
      return true;
    }

    const customMargins = this.getSettingValue('customMargins');
    return customMargins.marginTop !== undefined &&
        customMargins.marginLeft !== undefined &&
        customMargins.marginBottom !== undefined &&
        customMargins.marginRight !== undefined;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasTicketChanged_: function() {
    if (!this.marginsValid_()) {
      // Log so that we can try to debug how this occurs. See
      // https://crbug.com/942211
      console.warn('Requested preview with invalid margins');
      return false;
    }

    if (!this.lastTicket_) {
      return true;
    }

    const lastTicket = this.lastTicket_;

    // Margins
    const newMarginsType = this.getSettingValue('margins');
    if (newMarginsType !== lastTicket.marginsType &&
        newMarginsType !== print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
      return true;
    }

    if (newMarginsType === print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
      const customMargins =
          /** @type {!print_preview.MarginsSetting} */ (
              this.getSettingValue('customMargins'));

      // Change in custom margins values.
      if (!!lastTicket.marginsCustom &&
          (lastTicket.marginsCustom.marginTop != customMargins.marginTop ||
           lastTicket.marginsCustom.marginLeft != customMargins.marginLeft ||
           lastTicket.marginsCustom.marginRight != customMargins.marginRight ||
           lastTicket.marginsCustom.marginBottom !=
               customMargins.marginBottom)) {
        return true;
      }

      // Changed to custom margins from a different margins type.
      if (!this.margins) {
        // Log so that we can try to debug how this occurs. See
        // https://crbug.com/942211
        console.warn('Requested preview with undefined document margins');
        return false;
      }

      const customMarginsChanged =
          Object.values(print_preview.ticket_items.CustomMarginsOrientation)
              .some(side => {
                return this.margins.get(side) !==
                    customMargins[print_preview.MARGIN_KEY_MAP.get(side)];
              });
      if (customMarginsChanged) {
        return true;
      }
    }

    // Simple settings: ranges, layout, header/footer, pages per sheet, fit to
    // page, css background, selection only, rasterize, scaling, dpi
    if (!areRangesEqual(
            /** @type {!Array<{from: number, to: number}>} */ (
                this.getSettingValue('ranges')),
            lastTicket.pageRange) ||
        this.getSettingValue('layout') !== lastTicket.landscape ||
        this.getColorForTicket_() !== lastTicket.color ||
        this.getSettingValue('headerFooter') !==
            lastTicket.headerFooterEnabled ||
        this.getSettingValue('fitToPage') !== lastTicket.fitToPageEnabled ||
        this.getSettingValue('cssBackground') !==
            lastTicket.shouldPrintBackgrounds ||
        this.getSettingValue('selectionOnly') !==
            lastTicket.shouldPrintSelectionOnly ||
        this.getSettingValue('rasterize') !== lastTicket.rasterizePDF ||
        this.getScaleFactorForTicket_() !== lastTicket.scaleFactor) {
      return true;
    }

    // Pages per sheet. If margins are non-default, wait for the return to
    // default margins to trigger a request.
    if (this.getSettingValue('pagesPerSheet') !== lastTicket.pagesPerSheet &&
        this.getSettingValue('margins') ===
            print_preview.ticket_items.MarginsTypeValue.DEFAULT) {
      return true;
    }

    // Media size
    const newValue =
        /** @type {!print_preview.MediaSizeValue} */ (
            this.getSettingValue('mediaSize'));
    if (newValue.height_microns != lastTicket.mediaSize.height_microns ||
        newValue.width_microns != lastTicket.mediaSize.width_microns ||
        (this.destination.id !== lastTicket.deviceName &&
         this.getSettingValue('margins') ===
             print_preview.ticket_items.MarginsTypeValue.MINIMUM)) {
      return true;
    }

    // Destination
    if (this.destination.isPrivet !== lastTicket.printWithPrivet ||
        this.destination.isExtension !== lastTicket.printWithExtension ||
        !this.destination.isLocal !== lastTicket.printWithCloudPrint ||
        (lastTicket.printToPDF &&
         this.destination.id !==
             print_preview.Destination.GooglePromotedId.SAVE_AS_PDF)) {
      return true;
    }

    return false;
  },

  /** @return {number} Native color model of the destination. */
  getColorForTicket_: function() {
    return this.destination.getNativeColorModel(
        /** @type {boolean} */ (this.getSettingValue('color')));
  },

  /** @return {number} Scale factor. */
  getScaleFactorForTicket_: function() {
    return this.getSettingValue('customScaling') ?
        parseInt(this.getSettingValue('scaling'), 10) :
        100;
  },

  /**
   * @param {string} dpiField The field in dpi to retrieve.
   * @return {number} Field value.
   */
  getDpiForTicket_: function(dpiField) {
    const dpi =
        /**
           @type {{horizontal_dpi: (number | undefined),
                    vertical_dpi: (number | undefined),
                    vendor_id: (number | undefined)}}
         */
        (this.getSettingValue('dpi'));
    const value = (dpi && dpiField in dpi) ? dpi[dpiField] : 0;
    return value;
  },

  /**
   * Requests a preview from the native layer.
   * @return {!Promise} Promise that resolves when the preview has been
   *     generated.
   */
  getPreview_: function() {
    this.inFlightRequestId_++;
    const ticket = {
      pageRange: this.getSettingValue('ranges'),
      mediaSize: this.getSettingValue('mediaSize'),
      landscape: this.getSettingValue('layout'),
      color: this.getColorForTicket_(),
      headerFooterEnabled: this.getSettingValue('headerFooter'),
      marginsType: this.getSettingValue('margins'),
      pagesPerSheet: this.getSettingValue('pagesPerSheet'),
      isFirstRequest: this.inFlightRequestId_ == 0,
      requestID: this.inFlightRequestId_,
      previewModifiable: this.documentModifiable,
      fitToPageEnabled: this.getSettingValue('fitToPage'),
      scaleFactor: this.getScaleFactorForTicket_(),
      shouldPrintBackgrounds: this.getSettingValue('cssBackground'),
      shouldPrintSelectionOnly: this.getSettingValue('selectionOnly'),
      // NOTE: Even though the remaining fields don't directly relate to the
      // preview, they still need to be included.
      // e.g. printing::PrintSettingsFromJobSettings() still checks for them.
      collate: true,
      copies: 1,
      deviceName: this.destination.id,
      dpiHorizontal: this.getDpiForTicket_('horizontal_dpi'),
      dpiVertical: this.getDpiForTicket_('vertical_dpi'),
      duplex: this.getSettingValue('duplex') ?
          print_preview.DuplexMode.LONG_EDGE :
          print_preview.DuplexMode.SIMPLEX,
      printToPDF: this.destination.id ==
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
      printWithCloudPrint: !this.destination.isLocal,
      printWithPrivet: this.destination.isPrivet,
      printWithExtension: this.destination.isExtension,
      rasterizePDF: this.getSettingValue('rasterize'),
    };

    // Set 'cloudPrintID' only if the this.destination is not local.
    if (this.destination && !this.destination.isLocal) {
      ticket.cloudPrintID = this.destination.id;
    }

    if (this.getSettingValue('margins') ==
        print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
      ticket.marginsCustom = this.getSettingValue('customMargins');
    }
    this.lastTicket_ = ticket;

    this.fire('preview-start', this.inFlightRequestId_);
    return this.nativeLayer_.getPreview(JSON.stringify(ticket));
  },

  /** @private */
  onStateOrErrorChange_: function() {
    if ((this.state === print_preview.State.ERROR ||
         this.state === print_preview.State.FATAL_ERROR) &&
        this.getErrorMessage_() !== '') {
      this.previewState = print_preview.PreviewAreaState.ERROR;
    }
  },

  /** @return {string} The error message to display in the preview area. */
  getErrorMessage_: function() {
    switch (this.error) {
      case print_preview.Error.INVALID_PRINTER:
        return this.i18nAdvanced('invalidPrinterSettings', {
          substitutions: [],
          tags: ['BR'],
        });
      case print_preview.Error.UNSUPPORTED_PRINTER:
        return this.i18nAdvanced('unsupportedCloudPrinter', {
          substitutions: [],
          tags: ['BR'],
        });
      // <if expr="chromeos">
      case print_preview.Error.NO_DESTINATIONS:
        return this.i18n('noDestinationsMessage');
      // </if>
      case print_preview.Error.NO_PLUGIN:
        return this.i18n('noPlugin');
      case print_preview.Error.PREVIEW_FAILED:
        return this.i18n('previewFailed');
      default:
        return '';
    }
  },
});
