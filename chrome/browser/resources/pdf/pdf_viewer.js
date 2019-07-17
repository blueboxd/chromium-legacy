// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @typedef {{
 *   dataToSave: Array,
 *   token: string,
 *   fileName: string
 * }}
 */
let SaveDataMessageData;

/**
 * @return {number} Width of a scrollbar in pixels
 */
function getScrollbarWidth() {
  const div = document.createElement('div');
  div.style.visibility = 'hidden';
  div.style.overflow = 'scroll';
  div.style.width = '50px';
  div.style.height = '50px';
  div.style.position = 'absolute';
  document.body.appendChild(div);
  const result = div.offsetWidth - div.clientWidth;
  div.parentNode.removeChild(div);
  return result;
}

/**
 * Return the filename component of a URL, percent decoded if possible.
 *
 * @param {string} url The URL to get the filename from.
 * @return {string} The filename component.
 */
function getFilenameFromURL(url) {
  // Ignore the query and fragment.
  const mainUrl = url.split(/#|\?/)[0];
  const components = mainUrl.split(/\/|\\/);
  const filename = components[components.length - 1];
  try {
    return decodeURIComponent(filename);
  } catch (e) {
    if (e instanceof URIError) {
      return filename;
    }
    throw e;
  }
}

/**
 * Whether keydown events should currently be ignored. Events are ignored when
 * an editable element has focus, to allow for proper editing controls.
 *
 * @param {HTMLElement} activeElement The currently selected DOM node.
 * @return {boolean} True if keydown events should be ignored.
 */
function shouldIgnoreKeyEvents(activeElement) {
  while (activeElement.shadowRoot != null &&
         activeElement.shadowRoot.activeElement != null) {
    activeElement = activeElement.shadowRoot.activeElement;
  }

  return (
      activeElement.isContentEditable ||
      (activeElement.tagName == 'INPUT' && activeElement.type != 'radio') ||
      activeElement.tagName == 'TEXTAREA');
}

/**
 * Creates a cryptographically secure pseudorandom 128-bit token.
 *
 * @return {string} The generated token as a hex string.
 */
function createToken() {
  const randomBytes = new Uint8Array(16);
  return window.crypto.getRandomValues(randomBytes)
      .map(b => b.toString(16).padStart(2, '0'))
      .join('');
}

/**
 * The minimum number of pixels to offset the toolbar by from the bottom and
 * right side of the screen.
 */
PDFViewer.MIN_TOOLBAR_OFFSET = 15;

/**
 * The height of the toolbar along the top of the page. The document will be
 * shifted down by this much in the viewport.
 */
PDFViewer.MATERIAL_TOOLBAR_HEIGHT = 56;

/**
 * Minimum height for the material toolbar to show (px). Should match the media
 * query in index-material.css. If the window is smaller than this at load,
 * leave no space for the toolbar.
 */
PDFViewer.TOOLBAR_WINDOW_MIN_HEIGHT = 250;

/**
 * The background color used for print preview (--google-grey-refresh-300).
 */
PDFViewer.PRINT_PREVIEW_BACKGROUND_COLOR = '0xFFDADCE0';

/**
 * The background color used for print preview when dark mode is enabled
 * (--google-grey-refresh-700).
 */
PDFViewer.PRINT_PREVIEW_DARK_BACKGROUND_COLOR = '0xFF5F6368';

/**
 * The background color used for the regular viewer.
 */
PDFViewer.BACKGROUND_COLOR = '0xFF525659';

/**
 * Creates a new PDFViewer. There should only be one of these objects per
 * document.
 *
 * @param {!BrowserApi} browserApi An object providing an API to the browser.
 * @constructor
 */
function PDFViewer(browserApi) {
  this.browserApi_ = browserApi;
  this.originalUrl_ = this.browserApi_.getStreamInfo().originalUrl;
  this.javascript_ = this.browserApi_.getStreamInfo().javascript || 'block';
  this.loadState_ = LoadState.LOADING;
  this.parentWindow_ = null;
  this.parentOrigin_ = null;
  this.isFormFieldFocused_ = false;
  this.beepCount_ = 0;
  this.delayedScriptingMessages_ = [];
  this.loaded_ = new PromiseResolver();

  this.isPrintPreview_ = location.origin === 'chrome://print';
  this.isPrintPreviewLoadingFinished_ = false;
  this.isUserInitiatedEvent_ = true;

  /** @private {boolean} */
  this.hasEnteredAnnotationMode_ = false;

  /** @private {boolean} */
  this.hadPassword_ = false;

  /** @private {boolean} */
  this.canSerializeDocument_ = false;

  PDFMetrics.record(PDFMetrics.UserAction.DOCUMENT_OPENED);

  // Parse open pdf parameters.
  this.paramsParser_ = new OpenPDFParamsParser(
      message => this.pluginController_.postMessage(message));
  const toolbarEnabled =
      this.paramsParser_.getUiUrlParams(this.originalUrl_).toolbar &&
      !this.isPrintPreview_;

  // The sizer element is placed behind the plugin element to cause scrollbars
  // to be displayed in the window. It is sized according to the document size
  // of the pdf and zoom level.
  this.sizer_ = $('sizer');
  if (this.isPrintPreview_) {
    this.pageIndicator_ = $('page-indicator');
  }
  this.passwordScreen_ = $('password-screen');
  this.passwordScreen_.addEventListener(
      'password-submitted', this.onPasswordSubmitted_.bind(this));
  this.errorScreen_ = $('error-screen');
  // Can only reload if we are in a normal tab.
  if (chrome.tabs && this.browserApi_.getStreamInfo().tabId != -1) {
    this.errorScreen_.reloadFn = () => {
      chrome.tabs.reload(this.browserApi_.getStreamInfo().tabId);
    };
  }

  // Create the viewport.
  const shortWindow = window.innerHeight < PDFViewer.TOOLBAR_WINDOW_MIN_HEIGHT;
  const topToolbarHeight =
      (toolbarEnabled) ? PDFViewer.MATERIAL_TOOLBAR_HEIGHT : 0;
  const defaultZoom =
      this.browserApi_.getZoomBehavior() == BrowserApi.ZoomBehavior.MANAGE ?
      this.browserApi_.getDefaultZoom() :
      1.0;
  this.viewport_ = new ViewportImpl(
      window, this.sizer_, this.viewportChanged_.bind(this),
      () => this.currentController_.beforeZoom(),
      () => {
        this.currentController_.afterZoom();
        this.zoomManager_.onPdfZoomChange();
      },
      this.setUserInitiated_.bind(this), getScrollbarWidth(), defaultZoom,
      topToolbarHeight);

  // Create the plugin object dynamically so we can set its src. The plugin
  // element is sized to fill the entire window and is set to be fixed
  // positioning, acting as a viewport. The plugin renders into this viewport
  // according to the scroll position of the window.
  this.plugin_ = document.createElement('embed');
  // NOTE: The plugin's 'id' field must be set to 'plugin' since
  // chrome/renderer/printing/print_render_frame_helper.cc actually
  // references it.
  this.plugin_.id = 'plugin';
  this.plugin_.type = 'application/x-google-chrome-pdf';

  // Handle scripting messages from outside the extension that wish to interact
  // with it. We also send a message indicating that extension has loaded and
  // is ready to receive messages.
  window.addEventListener(
      'message', this.handleScriptingMessage.bind(this), false);

  this.plugin_.setAttribute('src', this.originalUrl_);
  this.plugin_.setAttribute(
      'stream-url', this.browserApi_.getStreamInfo().streamUrl);
  let headers = '';
  for (const header in this.browserApi_.getStreamInfo().responseHeaders) {
    headers += header + ': ' +
        this.browserApi_.getStreamInfo().responseHeaders[header] + '\n';
  }
  this.plugin_.setAttribute('headers', headers);

  this.plugin_.setAttribute('background-color', PDFViewer.BACKGROUND_COLOR);
  this.plugin_.setAttribute('top-toolbar-height', topToolbarHeight);
  this.plugin_.setAttribute('javascript', this.javascript_);

  if (this.browserApi_.getStreamInfo().embedded) {
    this.plugin_.setAttribute(
        'top-level-url', this.browserApi_.getStreamInfo().tabUrl);
  } else {
    this.plugin_.setAttribute('full-frame', '');
  }

  $('content').appendChild(this.plugin_);

  this.pluginController_ =
      new PluginController(this.plugin_, this, this.viewport_);
  this.inkController_ = new InkController(this, this.viewport_);
  this.currentController_ = this.pluginController_;

  // Setup the button event listeners.
  this.zoomToolbar_ = $('zoom-toolbar');
  this.zoomToolbar_.setIsPrintPreview(this.isPrintPreview_);
  this.zoomToolbar_.addEventListener(
      'fit-to-changed', this.fitToChanged_.bind(this));
  this.zoomToolbar_.addEventListener(
      'zoom-in', this.viewport_.zoomIn.bind(this.viewport_));
  this.zoomToolbar_.addEventListener(
      'zoom-out', this.viewport_.zoomOut.bind(this.viewport_));

  this.gestureDetector_ = new GestureDetector($('content'));
  this.gestureDetector_.addEventListener(
      'pinchstart', this.onPinchStart_.bind(this));
  this.sentPinchEvent_ = false;
  this.gestureDetector_.addEventListener(
      'pinchupdate', this.onPinchUpdate_.bind(this));
  this.gestureDetector_.addEventListener(
      'pinchend', this.onPinchEnd_.bind(this));

  if (toolbarEnabled) {
    this.toolbar_ = $('toolbar');
    this.toolbar_.hidden = false;
    this.toolbar_.addEventListener('save', () => this.save());
    this.toolbar_.addEventListener('print', () => this.print());
    this.toolbar_.addEventListener(
        'undo', () => this.currentController_.undo());
    this.toolbar_.addEventListener(
        'redo', () => this.currentController_.redo());
    this.toolbar_.addEventListener(
        'rotate-right', () => this.rotateClockwise());
    this.toolbar_.addEventListener(
        'annotation-mode-toggled', e => this.annotationModeToggled_(e));
    this.toolbar_.addEventListener(
        'annotation-tool-changed',
        e => this.inkController_.setAnnotationTool(e.detail.value));

    this.toolbar_.docTitle = getFilenameFromURL(this.originalUrl_);
  }

  document.body.addEventListener('change-page', e => {
    this.viewport_.goToPage(e.detail.page);
    if (e.detail.origin == 'bookmark') {
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
    } else if (e.detail.origin == 'pageselector') {
      PDFMetrics.record(PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE);
    }
  });

  document.body.addEventListener('change-page-and-xy', e => {
    const point = this.viewport_.convertPageToScreen(e.detail.page, e.detail);
    this.goToPageAndXY_(e.detail.origin, e.detail.page, point);
  });

  document.body.addEventListener('navigate', e => {
    const disposition = e.detail.newtab ?
        Navigator.WindowOpenDisposition.NEW_BACKGROUND_TAB :
        Navigator.WindowOpenDisposition.CURRENT_TAB;
    this.navigator_.navigate(e.detail.uri, disposition);
  });

  document.body.addEventListener('dropdown-opened', e => {
    if (e.detail == 'bookmarks') {
      PDFMetrics.record(PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL);
    }
  });

  this.toolbarManager_ =
      new ToolbarManager(window, this.toolbar_, this.zoomToolbar_);

  // Set up the ZoomManager.
  this.zoomManager_ = ZoomManager.create(
      this.browserApi_.getZoomBehavior(), this.viewport_,
      this.browserApi_.setZoom.bind(this.browserApi_),
      this.browserApi_.getInitialZoom());
  this.viewport_.zoomManager = this.zoomManager_;
  this.browserApi_.addZoomEventListener(
      this.zoomManager_.onBrowserZoomChange.bind(this.zoomManager_));

  // Setup the keyboard event listener.
  document.addEventListener('keydown', this.handleKeyEvent_.bind(this));
  document.addEventListener('mousemove', this.handleMouseEvent_.bind(this));
  document.addEventListener('mouseout', this.handleMouseEvent_.bind(this));
  document.addEventListener(
      'contextmenu', this.handleContextMenuEvent_.bind(this));

  const tabId = this.browserApi_.getStreamInfo().tabId;
  this.navigator_ = new Navigator(
      this.originalUrl_, this.viewport_, this.paramsParser_,
      new NavigatorDelegate(tabId));
  this.viewportScroller_ =
      new ViewportScroller(this.viewport_, this.plugin_, window);

  // Request translated strings.
  chrome.resourcesPrivate.getStrings('pdf', this.handleStrings_.bind(this));

  // Listen for save commands from the browser.
  if (chrome.mimeHandlerPrivate && chrome.mimeHandlerPrivate.onSave) {
    chrome.mimeHandlerPrivate.onSave.addListener(this.onSave.bind(this));
  }
}

PDFViewer.prototype = {
  /**
   * Handle key events. These may come from the user directly or via the
   * scripting API.
   *
   * @param {KeyboardEvent} e the event to handle.
   * @private
   */
  handleKeyEvent_: function(e) {
    const position = this.viewport_.position;
    // Certain scroll events may be sent from outside of the extension.
    const fromScriptingAPI = e.fromScriptingAPI;

    if (shouldIgnoreKeyEvents(document.activeElement) || e.defaultPrevented) {
      return;
    }

    this.toolbarManager_.hideToolbarsAfterTimeout(e);

    const pageUpHandler = () => {
      // Go to the previous page if we are fit-to-page or fit-to-height.
      if (this.viewport_.isPagedMode()) {
        this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y -= this.viewport.size.height;
        this.viewport.position = position;
      }
    };
    const pageDownHandler = () => {
      // Go to the next page if we are fit-to-page or fit-to-height.
      if (this.viewport_.isPagedMode()) {
        this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y += this.viewport.size.height;
        this.viewport.position = position;
      }
    };

    switch (e.keyCode) {
      case 9:  // Tab key.
        this.toolbarManager_.showToolbarsForKeyboardNavigation();
        return;
      case 27:  // Escape key.
        if (!this.isPrintPreview_) {
          this.toolbarManager_.hideSingleToolbarLayer();
          return;
        }
        break;  // Ensure escape falls through to the print-preview handler.
      case 32:  // Space key.
        if (e.shiftKey) {
          pageUpHandler();
        } else {
          pageDownHandler();
        }
        return;
      case 33:  // Page up key.
        pageUpHandler();
        return;
      case 34:  // Page down key.
        pageDownHandler();
        return;
      case 37:  // Left arrow key.
        if (!hasKeyModifiers(e)) {
          // Go to the previous page if there are no horizontal scrollbars and
          // no form field is focused.
          if (!(this.viewport_.documentHasScrollbars().horizontal ||
                this.isFormFieldFocused_)) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
            // Since we do the movement of the page.
            e.preventDefault();
          } else if (fromScriptingAPI) {
            position.x -= Viewport.SCROLL_INCREMENT;
            this.viewport.position = position;
          }
        }
        return;
      case 38:  // Up arrow key.
        if (fromScriptingAPI) {
          position.y -= Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 39:  // Right arrow key.
        if (!hasKeyModifiers(e)) {
          // Go to the next page if there are no horizontal scrollbars and no
          // form field is focused.
          if (!(this.viewport_.documentHasScrollbars().horizontal ||
                this.isFormFieldFocused_)) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
            // Since we do the movement of the page.
            e.preventDefault();
          } else if (fromScriptingAPI) {
            position.x += Viewport.SCROLL_INCREMENT;
            this.viewport.position = position;
          }
        }
        return;
      case 40:  // Down arrow key.
        if (fromScriptingAPI) {
          position.y += Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 65:  // 'a' key.
        if (e.ctrlKey || e.metaKey) {
          this.pluginController_.postMessage({type: 'selectAll'});
          // Since we do selection ourselves.
          e.preventDefault();
        }
        return;
      case 71:  // 'g' key.
        if (this.toolbar_ && (e.ctrlKey || e.metaKey) && e.altKey) {
          this.toolbarManager_.showToolbars();
          this.toolbar_.selectPageNumber();
        }
        return;
      case 219:  // Left bracket key.
        if (e.ctrlKey) {
          this.rotateCounterclockwise();
        }
        return;
      case 220:  // Backslash key.
        if (e.ctrlKey) {
          this.zoomToolbar_.fitToggleFromHotKey();
        }
        return;
      case 221:  // Right bracket key.
        if (e.ctrlKey) {
          this.rotateClockwise();
        }
        return;
    }

    // Give print preview a chance to handle the key event.
    if (!fromScriptingAPI && this.isPrintPreview_) {
      this.sendScriptingMessage_(
          {type: 'sendKeyEvent', keyEvent: SerializeKeyEvent(e)});
    } else {
      // Show toolbars as a fallback.
      if (!(e.shiftKey || e.ctrlKey || e.altKey)) {
        this.toolbarManager_.showToolbars();
      }
    }
  },

  handleMouseEvent_: function(e) {
    if (e.type == 'mousemove') {
      this.toolbarManager_.handleMouseMove(e);
    } else if (e.type == 'mouseout') {
      this.toolbarManager_.hideToolbarsForMouseOut();
    }
  },

  handleContextMenuEvent_: function(e) {
    // Stop Chrome from popping up the context menu on long press. We need to
    // make sure the start event did not have 2 touches because we don't want
    // to block two finger tap opening the context menu. We check for
    // firesTouchEvents in order to not block the context menu on right click.
    if (e.sourceCapabilities.firesTouchEvents &&
        !this.gestureDetector_.wasTwoFingerTouch()) {
      e.preventDefault();
    }
  },

  /**
   * Handles the annotation mode being toggled on or off.
   *
   * @param {!CustomEvent<{value: boolean}>} e
   * @private
   */
  annotationModeToggled_: async function(e) {
    const annotationMode = e.detail.value;
    if (annotationMode) {
      // Enter annotation mode.
      assert(this.currentController_ == this.pluginController_);
      // TODO(dstockwell): set plugin read-only, begin transition
      this.updateProgress(0);
      // TODO(dstockwell): handle save failure
      const result = await this.pluginController_.save(true);
      if (result.hasUnsavedChanges) {
        assert(!loadTimeData.getBoolean('pdfFormSaveEnabled'));
        try {
          await $('form-warning').show();
        } catch (e) {
          // The user aborted entering annotation mode. Revert to the plugin.
          this.toolbar_.annotationMode = false;
          this.updateProgress(100);
          return;
        }
      }
      PDFMetrics.record(PDFMetrics.UserAction.ENTER_ANNOTATION_MODE);
      this.hasEnteredAnnotationMode_ = true;
      // TODO(dstockwell): feed real progress data from the Ink component
      this.updateProgress(50);
      await this.inkController_.load(result.fileName, result.dataToSave);
      this.inkController_.setAnnotationTool(this.toolbar_.annotationTool);
      this.currentController_ = this.inkController_;
      this.pluginController_.unload();
      this.updateProgress(100);
    } else {
      // Exit annotation mode.
      PDFMetrics.record(PDFMetrics.UserAction.EXIT_ANNOTATION_MODE);
      assert(this.currentController_ == this.inkController_);
      // TODO(dstockwell): set ink read-only, begin transition
      this.updateProgress(0);
      // This runs separately to allow other consumers of `loaded` to queue
      // up after this task.
      this.loaded.then(() => {
        this.currentController_ = this.pluginController_;
        this.inkController_.unload();
      });
      // TODO(dstockwell): handle save failure
      const result = await this.inkController_.save(true);
      await this.pluginController_.load(result.fileName, result.dataToSave);
      // Ensure the plugin gets the initial viewport.
      this.viewport_.setZoom(this.viewport_.zoom);
    }
  },

  /**
   * Exits annotation mode if active.
   *
   * @return {Promise<void>}
   */
  exitAnnotationMode_: async function() {
    if (!this.toolbar_.annotationMode) {
      return;
    }
    this.toolbar_.toggleAnnotation();
    await this.loaded;
  },

  /**
   * Request to change the viewport fitting type.
   *
   * @param {!CustomEvent<{
   *     fittingType: FittingType,
   *     userInitiated: boolean
   * }>} e
   * @private
   */
  fitToChanged_: function(e) {
    if (e.detail.fittingType == FittingType.FIT_TO_PAGE) {
      this.viewport_.fitToPage();
      this.toolbarManager_.forceHideTopToolbar();
    } else if (e.detail.fittingType == FittingType.FIT_TO_WIDTH) {
      this.viewport_.fitToWidth();
    } else if (e.detail.fittingType == FittingType.FIT_TO_HEIGHT) {
      this.viewport_.fitToHeight();
      this.toolbarManager_.forceHideTopToolbar();
    }

    if (e.detail.userInitiated) {
      PDFMetrics.recordFitTo(e.detail.fittingType);
    }
  },

  /**
   * Sends a 'documentLoaded' message to the PDFScriptingAPI if the document has
   * finished loading.
   *
   * @private
   */
  sendDocumentLoadedMessage_: function() {
    if (this.loadState_ == LoadState.LOADING) {
      return;
    }
    if (this.isPrintPreview_ && !this.isPrintPreviewLoadingFinished_) {
      return;
    }
    this.sendScriptingMessage_(
        {type: 'documentLoaded', load_state: this.loadState_});
  },

  /**
   * Handle open pdf parameters. This function updates the viewport as per
   * the parameters mentioned in the url while opening pdf. The order is
   * important as later actions can override the effects of previous actions.
   *
   * @param {Object} params The open params passed in the URL.
   * @private
   */
  handleURLParams_: function(params) {
    if (params.zoom) {
      this.viewport_.setZoom(params.zoom);
    }

    if (params.position) {
      this.viewport_.goToPageAndXY(
          params.page ? params.page : 0, params.position.x, params.position.y);
    } else if (params.page) {
      this.viewport_.goToPage(params.page);
    }

    if (params.view) {
      this.isUserInitiatedEvent_ = false;
      this.zoomToolbar_.forceFit(params.view);
      if (params.viewPosition) {
        const zoomedPositionShift = params.viewPosition * this.viewport_.zoom;
        const currentViewportPosition = this.viewport_.position;
        if (params.view == FittingType.FIT_TO_WIDTH) {
          currentViewportPosition.y += zoomedPositionShift;
        } else if (params.view == FittingType.FIT_TO_HEIGHT) {
          currentViewportPosition.x += zoomedPositionShift;
        }
        this.viewport_.position = currentViewportPosition;
      }
      this.isUserInitiatedEvent_ = true;
    }
  },

  /**
   * Moves the viewport to a point in a page. Called back after a
   * 'transformPagePointReply' is returned from the plugin.
   *
   * @param {string} origin Identifier for the caller for logging purposes.
   * @param {number} page The index of the page to go to. zero-based.
   * @param {Object} message Message received from the plugin containing the
   *     x and y to navigate to in screen coordinates.
   * @private
   */
  goToPageAndXY_: function(origin, page, message) {
    this.viewport_.goToPageAndXY(page, message.x, message.y);
    if (origin == 'bookmark') {
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
    }
  },

  /**
   * @return {Promise} Resolved when the load state reaches LOADED,
   *     rejects on FAILED.
   */
  get loaded() {
    return this.loaded_.promise;
  },

  /**
   * Updates the load state and triggers completion of the `loaded`
   * promise if necessary.
   * @param {!LoadState} loadState
   * @private
   */
  setLoadState_(loadState) {
    if (this.loadState_ == loadState) {
      return;
    }
    if (loadState == LoadState.SUCCESS) {
      assert(this.loadState_ == LoadState.LOADING);
      this.loaded_.resolve();
    } else if (loadState == LoadState.FAILED) {
      assert(this.loadState_ == LoadState.LOADING);
      this.loaded_.reject();
    } else {
      assert(loadState == LoadState.LOADING);
      this.loaded_ = new PromiseResolver();
    }
    this.loadState_ = loadState;
  },

  /**
   * Update the loading progress of the document in response to a progress
   * message being received from the content controller.
   *
   * @param {number} progress the progress as a percentage.
   */
  updateProgress: function(progress) {
    if (this.toolbar_) {
      this.toolbar_.loadProgress = progress;
    }

    if (progress == -1) {
      // Document load failed.
      this.errorScreen_.show();
      this.sizer_.style.display = 'none';
      if (this.passwordScreen_.active) {
        this.passwordScreen_.deny();
        this.passwordScreen_.close();
      }
      this.setLoadState_(LoadState.FAILED);
      this.isPrintPreviewLoadingFinished_ = true;
      this.sendDocumentLoadedMessage_();
    } else if (progress == 100) {
      // Document load complete.
      if (this.lastViewportPosition_) {
        this.viewport_.position = this.lastViewportPosition_;
      }
      this.paramsParser_.getViewportFromUrlParams(
          this.originalUrl_, this.handleURLParams_.bind(this));
      this.setLoadState_(LoadState.SUCCESS);
      this.sendDocumentLoadedMessage_();
      while (this.delayedScriptingMessages_.length > 0) {
        this.handleScriptingMessage(this.delayedScriptingMessages_.shift());
      }

      this.toolbarManager_.hideToolbarsAfterTimeout();
    } else {
      this.setLoadState_(LoadState.LOADING);
    }
  },

  /** @private */
  sendBackgroundColorForPrintPreview_: function() {
    this.pluginController_.postMessage({
      type: 'backgroundColorChanged',
      backgroundColor: this.dark_ ?
          PDFViewer.PRINT_PREVIEW_DARK_BACKGROUND_COLOR :
          PDFViewer.PRINT_PREVIEW_BACKGROUND_COLOR,
    });
  },

  /**
   * Load a dictionary of translated strings into the UI. Used as a callback for
   * chrome.resourcesPrivate.
   *
   * @param {Object} strings Dictionary of translated strings
   * @private
   */
  handleStrings_: function(strings) {
    document.documentElement.dir = strings.textdirection;
    document.documentElement.lang = strings.language;

    loadTimeData.data = strings;
    const isNewPrintPreview = this.isPrintPreview_ &&
        loadTimeData.getBoolean('newPrintPreviewLayoutEnabled');
    if (isNewPrintPreview) {
      this.sendBackgroundColorForPrintPreview_();
      this.toolbarManager_.reverseSideToolbar();
    }
    this.reverseZoomToolbar_ = isNewPrintPreview;
    this.zoomToolbar_.newPrintPreview = isNewPrintPreview;

    $('toolbar').strings = strings;
    $('toolbar').pdfAnnotationsEnabled =
        loadTimeData.getBoolean('pdfAnnotationsEnabled');
    $('toolbar').printingEnabled = loadTimeData.getBoolean('printingEnabled');
    $('zoom-toolbar').setStrings(strings);
    $('password-screen').strings = strings;
    $('error-screen').strings = strings;
    if ($('form-warning')) {
      $('form-warning').strings = strings;
    }
  },

  /**
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password screen.
   *
   * @param {Object} event a password-submitted event.
   * @private
   */
  onPasswordSubmitted_: function(event) {
    this.pluginController_.postMessage(
        {type: 'getPasswordComplete', password: event.detail.password});
  },

  /**
   * A callback that sets |isUserInitiatedEvent_| to |userInitiated|.
   *
   * @param {boolean} userInitiated The value to set |isUserInitiatedEvent_| to.
   * @private
   */
  setUserInitiated_: function(userInitiated) {
    assert(this.isUserInitiatedEvent_ != userInitiated);
    this.isUserInitiatedEvent_ = userInitiated;
  },

  /**
   * A callback that's called when an update to a pinch zoom is detected.
   *
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchUpdate_: function(e) {
    // Throttle number of pinch events to one per frame.
    if (!this.sentPinchEvent_) {
      this.sentPinchEvent_ = true;
      window.requestAnimationFrame(() => {
        this.sentPinchEvent_ = false;
        this.viewport_.pinchZoom(e);
      });
    }
  },

  /**
   * A callback that's called when the end of a pinch zoom is detected.
   *
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchEnd_: function(e) {
    // Using rAF for pinch end prevents pinch updates scheduled by rAF getting
    // sent after the pinch end.
    window.requestAnimationFrame(() => {
      this.viewport_.pinchZoomEnd(e);
    });
  },

  /**
   * A callback that's called when the start of a pinch zoom is detected.
   *
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchStart_: function(e) {
    // We also use rAF for pinch start, so that if there is a pinch end event
    // scheduled by rAF, this pinch start will be sent after.
    window.requestAnimationFrame(() => {
      this.viewport_.pinchZoomStart(e);
    });
  },

  /**
   * A callback that's called after the viewport changes.
   *
   * @private
   */
  viewportChanged_: function() {
    if (!this.documentDimensions_) {
      return;
    }

    // Offset the toolbar position so that it doesn't move if scrollbars appear.
    const hasScrollbars = this.viewport_.documentHasScrollbars();
    const scrollbarWidth = this.viewport_.scrollbarWidth;
    const verticalScrollbarWidth = hasScrollbars.vertical ? scrollbarWidth : 0;
    const horizontalScrollbarWidth =
        hasScrollbars.horizontal ? scrollbarWidth : 0;

    // Shift the zoom toolbar to the left by half a scrollbar width. This
    // gives a compromise: if there is no scrollbar visible then the toolbar
    // will be half a scrollbar width further left than the spec but if there
    // is a scrollbar visible it will be half a scrollbar width further right
    // than the spec. In RTL layout normally, and in LTR layout in Print Preview
    // when the NewPrintPreview flag is enabled, the zoom toolbar is on the left
    // left side, but the scrollbar is still on the right, so this is not
    // necessary.
    if (isRTL() === this.reverseZoomToolbar_) {
      this.zoomToolbar_.style.right =
          -verticalScrollbarWidth + (scrollbarWidth / 2) + 'px';
    }
    // Having a horizontal scrollbar is much rarer so we don't offset the
    // toolbar from the bottom any more than what the spec says. This means
    // that when there is a scrollbar visible, it will be a full scrollbar
    // width closer to the bottom of the screen than usual, but this is ok.
    this.zoomToolbar_.style.bottom = -horizontalScrollbarWidth + 'px';

    // Update the page indicator.
    const visiblePage = this.viewport_.getMostVisiblePage();

    if (this.toolbar_) {
      this.toolbar_.pageNo = visiblePage + 1;
    }

    // TODO(raymes): Give pageIndicator_ the same API as toolbar_.
    if (this.pageIndicator_) {
      this.pageIndicator_.index = visiblePage;
      if (this.documentDimensions_.pageDimensions.length > 1 &&
          hasScrollbars.vertical) {
        this.pageIndicator_.style.visibility = 'visible';
      } else {
        this.pageIndicator_.style.visibility = 'hidden';
      }
    }

    this.currentController_.viewportChanged();

    const visiblePageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    const size = this.viewport_.size;
    this.sendScriptingMessage_({
      type: 'viewport',
      pageX: visiblePageDimensions.x,
      pageY: visiblePageDimensions.y,
      pageWidth: visiblePageDimensions.width,
      viewportWidth: size.width,
      viewportHeight: size.height
    });
  },

  /**
   * Handle a scripting message from outside the extension (typically sent by
   * PDFScriptingAPI in a page containing the extension) to interact with the
   * plugin.
   *
   * @param {MessageObject} message the message to handle.
   */
  handleScriptingMessage: function(message) {
    if (this.parentWindow_ != message.source) {
      this.parentWindow_ = message.source;
      this.parentOrigin_ = message.origin;
      // Ensure that we notify the embedder if the document is loaded.
      if (this.loadState_ != LoadState.LOADING) {
        this.sendDocumentLoadedMessage_();
      }
    }

    if (this.handlePrintPreviewScriptingMessage_(message)) {
      return;
    }

    // Delay scripting messages from users of the scripting API until the
    // document is loaded. This simplifies use of the APIs.
    if (this.loadState_ != LoadState.SUCCESS) {
      this.delayedScriptingMessages_.push(message);
      return;
    }

    switch (message.data.type.toString()) {
      case 'getSelectedText':
      case 'print':
      case 'selectAll':
        this.pluginController_.postMessage(message.data);
        break;
    }
  },

  /**
   * Handle scripting messages specific to print preview.
   *
   * @param {MessageObject} message the message to handle.
   * @return {boolean} true if the message was handled, false otherwise.
   * @private
   */
  handlePrintPreviewScriptingMessage_: function(message) {
    if (!this.isPrintPreview_) {
      return false;
    }

    switch (message.data.type.toString()) {
      case 'loadPreviewPage':
        this.pluginController_.postMessage(message.data);
        return true;
      case 'resetPrintPreviewMode':
        this.setLoadState_(LoadState.LOADING);
        if (!this.inPrintPreviewMode_) {
          this.inPrintPreviewMode_ = true;
          this.isUserInitiatedEvent_ = false;
          this.zoomToolbar_.forceFit(FittingType.FIT_TO_PAGE);
          this.isUserInitiatedEvent_ = true;
        }

        // Stash the scroll location so that it can be restored when the new
        // document is loaded.
        this.lastViewportPosition_ = this.viewport_.position;

        // TODO(raymes): Disable these properly in the plugin.
        const printButton = $('print-button');
        if (printButton) {
          printButton.parentNode.removeChild(printButton);
        }
        const saveButton = $('save-button');
        if (saveButton) {
          saveButton.parentNode.removeChild(saveButton);
        }

        this.pageIndicator_.pageLabels = message.data.pageNumbers;

        this.pluginController_.postMessage({
          type: 'resetPrintPreviewMode',
          url: message.data.url,
          grayscale: message.data.grayscale,
          // If the PDF isn't modifiable we send 0 as the page count so that no
          // blank placeholder pages get appended to the PDF.
          pageCount:
              (message.data.modifiable ? message.data.pageNumbers.length : 0)
        });
        return true;
      case 'sendKeyEvent':
        this.handleKeyEvent_(DeserializeKeyEvent(message.data.keyEvent));
        return true;
      case 'hideToolbars':
        this.toolbarManager_.resetKeyboardNavigationAndHideToolbars();
        return true;
      case 'darkModeChanged':
        this.dark_ = message.data.darkMode;
        if (this.isPrintPreview_) {
          this.sendBackgroundColorForPrintPreview_();
        }
        return true;
      case 'scrollPosition':
        const position = this.viewport_.position;
        position.y += message.data.y;
        position.x += message.data.x;
        this.viewport.position = position;
        return true;
    }

    return false;
  },

  /**
   * Send a scripting message outside the extension (typically to
   * PDFScriptingAPI in a page containing the extension).
   *
   * @param {Object} message the message to send.
   * @private
   */
  sendScriptingMessage_: function(message) {
    if (this.parentWindow_ && this.parentOrigin_) {
      let targetOrigin;
      // Only send data back to the embedder if it is from the same origin,
      // unless we're sending it to ourselves (which could happen in the case
      // of tests). We also allow documentLoaded messages through as this won't
      // leak important information.
      if (this.parentOrigin_ == window.location.origin) {
        targetOrigin = this.parentOrigin_;
      } else if (message.type == 'documentLoaded') {
        targetOrigin = '*';
      } else {
        targetOrigin = this.originalUrl_;
      }
      this.parentWindow_.postMessage(message, targetOrigin);
    }
  },

  /**
   * @type {Viewport} the viewport of the PDF viewer.
   */
  get viewport() {
    return this.viewport_;
  },

  /**
   * Each bookmark is an Object containing a:
   * - title
   * - page (optional)
   * - array of children (themselves bookmarks)
   *
   * @type {Array} the top-level bookmarks of the PDF.
   */
  get bookmarks() {
    return this.bookmarks_;
  },

  /**
   * Sets document dimensions from the current controller.
   *
   * @param {{height: number, width: number, pageDimensions: Array}}
   *     documentDimensions
   */
  setDocumentDimensions: function(documentDimensions) {
    this.documentDimensions_ = documentDimensions;
    this.isUserInitiatedEvent_ = false;
    this.viewport_.setDocumentDimensions(this.documentDimensions_);
    this.isUserInitiatedEvent_ = true;
    // If we received the document dimensions, the password was good so we
    // can dismiss the password screen.
    if (this.passwordScreen_.active) {
      this.passwordScreen_.close();
    }

    if (this.toolbar_) {
      this.toolbar_.docLength = this.documentDimensions_.pageDimensions.length;
    }
  },

  /**
   * Handles a beep request from the current controller.
   */
  handleBeep: function() {
    // Beeps are annoying, so just track count for now.
    this.beepCount_ += 1;
  },

  /**
   * Handles a password request from the current controller.
   */
  handlePasswordRequest: function() {
    // If the password screen isn't up, put it up. Otherwise we're
    // responding to an incorrect password so deny it.
    if (!this.passwordScreen_.active) {
      this.hadPassword_ = true;
      this.updateAnnotationAvailable_();
      this.passwordScreen_.show();
    } else {
      this.passwordScreen_.deny();
    }
  },

  /**
   * Handles a selected text reply from the current controller.
   * @param {string} selectedText
   */
  handleSelectedTextReply: function(selectedText) {
    this.sendScriptingMessage_({
      type: 'getSelectedTextReply',
      selectedText: selectedText,
    });
  },

  /**
   * Handles a navigation request from the current controller.
   *
   * @param {string} url
   * @param {string} disposition
   */
  handleNavigate: function(url, disposition) {
    // If in print preview, always open a new tab.
    if (this.isPrintPreview_) {
      this.navigator_.navigate(url, Navigator.WindowOpenDisposition.NEW_BACKGROUND_TAB);
    } else {
      this.navigator_.navigate(url, disposition);
    }
  },

  /**
   * Handles a notification that print preview has loaded from the
   * current controller.
   */
  handlePrintPreviewLoaded: function() {
    this.isPrintPreviewLoadingFinished_ = true;
    this.sendDocumentLoadedMessage_();
  },

  /**
   * Sets document metadata from the current controller.
   * @param {string} title
   * @param {Array} bookmarks
   * @param {boolean} canSerializeDocument
   */
  setDocumentMetadata: function(title, bookmarks, canSerializeDocument) {
    if (title) {
      document.title = title;
    } else {
      document.title = getFilenameFromURL(this.originalUrl_);
    }
    this.bookmarks_ = bookmarks;
    if (this.toolbar_) {
      this.toolbar_.docTitle = document.title;
      this.toolbar_.bookmarks = this.bookmarks;
    }
    this.canSerializeDocument_ = canSerializeDocument;
    this.updateAnnotationAvailable_();
  },

  /**
   * Sets the is selecting flag from the current controller.
   * @param {boolean} isSelecting
   */
  setIsSelecting: function(isSelecting) {
    this.viewportScroller_.setEnableScrolling(isSelecting);
  },

  /**
   * Sets the form field focused flag from the current controller.
   * @param {boolean} focused
   */
  setIsFormFieldFocused: function(focused) {
    this.isFormFieldFocused_ = focused;
  },

  /**
   * An event handler for when the browser tells the PDF Viewer to perform a
   * save.
   *
   * @param {string} streamUrl unique identifier for a PDF Viewer instance.
   * @private
   */
  onSave: async function(streamUrl) {
    if (streamUrl != this.browserApi_.getStreamInfo().streamUrl) {
      return;
    }

    this.save();
  },

  /**
   * Saves the current PDF document to disk.
   */
  save: async function() {
    PDFMetrics.record(PDFMetrics.UserAction.SAVE);
    if (this.hasEnteredAnnotationMode_) {
      PDFMetrics.record(PDFMetrics.UserAction.SAVE_WITH_ANNOTATION);
    }
    // If we have entered annotation mode we must require the local
    // contents to ensure annotations are saved. Otherwise we would
    // save the cached or remote copy without annotatios.
    const requireResult = this.hasEnteredAnnotationMode_;
    // TODO(dstockwell): Report an error to user if this fails.
    const result = await this.currentController_.save(requireResult);
    if (result == null) {
      // The content controller handled the save internally.
      return;
    }

    // Make sure file extension is .pdf, avoids dangerous extensions.
    let fileName = result.fileName;
    if (!fileName.toLowerCase().endsWith('.pdf')) {
      fileName = fileName + '.pdf';
    }

    chrome.fileSystem.chooseEntry(
        {type: 'saveFile', suggestedName: fileName}, entry => {
          if (chrome.runtime.lastError) {
            if (chrome.runtime.lastError.message != 'User cancelled') {
              console.log(
                  'chrome.fileSystem.chooseEntry failed: ' +
                  chrome.runtime.lastError.message);
            }
            return;
          }
          entry.createWriter(writer => {
            writer.write(
                new Blob([result.dataToSave], {type: 'application/pdf'}));
            // Unblock closing the window now that the user has saved
            // successfully.
            chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(false);
          });
        });

    // Saving in Annotation mode is destructive: crbug.com/919364
    this.exitAnnotationMode_();
  },

  print: async function() {
    PDFMetrics.record(PDFMetrics.UserAction.PRINT);
    await this.exitAnnotationMode_();
    this.currentController_.print();
  },

  /**
   * Updates the toolbar's annotation available flag depending on current
   * conditions.
   */
  updateAnnotationAvailable_() {
    if (!this.toolbar_) {
      return;
    }
    let annotationAvailable = true;
    if (this.viewport_.getClockwiseRotations() != 0) {
      annotationAvailable = false;
    }
    if (this.hadPassword_) {
      annotationAvailable = false;
    }
    if (!this.canSerializeDocument_) {
      annotationAvailable = false;
    }
    this.toolbar_.annotationAvailable = annotationAvailable;
  },

  rotateClockwise() {
    PDFMetrics.record(PDFMetrics.UserAction.ROTATE);
    this.viewport_.rotateClockwise(1);
    this.currentController_.rotateClockwise();
    this.updateAnnotationAvailable_();
  },

  rotateCounterclockwise() {
    PDFMetrics.record(PDFMetrics.UserAction.ROTATE);
    this.viewport_.rotateClockwise(3);
    this.currentController_.rotateCounterclockwise();
    this.updateAnnotationAvailable_();
  },

  setHasUnsavedChanges: function() {
    // Warn the user if they attempt to close the window without saving.
    chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(true);
  },

  /** @param {UndoState} state */
  setAnnotationUndoState(state) {
    this.toolbar_.canUndoAnnotation = state.canUndo;
    this.toolbar_.canRedoAnnotation = state.canRedo;
  }
};

/** @abstract */
class ContentController {
  constructor() {}

  /**
   * A callback that's called before the zoom changes.
   */
  beforeZoom() {}

  /**
   * A callback that's called after the zoom changes.
   */
  afterZoom() {}

  /**
   * Handles a change to the viewport.
   */
  viewportChanged() {}

  /**
   * Rotates the document 90 degrees in the clockwise direction.
   * @abstract
   */
  rotateClockwise() {}

  /**
   * Rotates the document 90 degrees in the counter clockwise direction.
   * @abstract
   */
  rotateCounterclockwise() {}

  /**
   * Triggers printing of the current document.
   */
  print() {}

  /**
   * Undo an edit action.
   */
  undo() {}

  /**
   * Redo an edit action.
   */
  redo() {}

  /**
   * Requests that the current document be saved.
   * @param {boolean} requireResult whether a response is required, otherwise
   *     the controller may save the document to disk internally.
   * @return {Promise<{fileName: string, dataToSave: ArrayBuffer}}
   * @abstract
   */
  save(requireResult) {}

  /**
   * Loads PDF document from `data` activates UI.
   * @param {string} fileName
   * @param {ArrayBuffer} data
   * @return {Promise<void>}
   * @abstract
   */
  load(fileName, data) {}

  /**
   * Unloads the current document and removes the UI.
   * @abstract
   */
  unload() {}
}

class InkController extends ContentController {
  /**
   * @param {PDFViewer} viewer
   * @param {Viewport} viewport
   */
  constructor(viewer, viewport) {
    super();
    this.viewer_ = viewer;
    this.viewport_ = viewport;

    /** @type {ViewerInkHost} */
    this.inkHost_ = null;
  }

  /** @param {AnnotationTool} tool */
  setAnnotationTool(tool) {
    this.tool_ = tool;
    if (this.inkHost_) {
      this.inkHost_.setAnnotationTool(tool);
    }
  }

  /** @override */
  rotateClockwise() {
    // TODO(dstockwell): implement rotation
  }

  /** @override */
  rotateCounterclockwise() {
    // TODO(dstockwell): implement rotation
  }

  /** @override */
  viewportChanged() {
    this.inkHost_.viewportChanged();
  }

  /** @override */
  save(requireResult) {
    return this.inkHost_.saveDocument();
  }

  /** @override */
  undo() {
    this.inkHost_.undo();
  }

  /** @override */
  redo() {
    this.inkHost_.redo();
  }

  /** @override */
  load(filename, data) {
    if (!this.inkHost_) {
      this.inkHost_ = document.createElement('viewer-ink-host');
      $('content').appendChild(this.inkHost_);
      this.inkHost_.viewport = this.viewport_;
      this.inkHost_.addEventListener('stroke-added', e => {
        this.viewer_.setHasUnsavedChanges();
      });
      this.inkHost_.addEventListener('undo-state-changed', e => {
        this.viewer_.setAnnotationUndoState(e.detail);
      });
    }
    return this.inkHost_.load(filename, data);
  }

  /** @override */
  unload() {
    this.inkHost_.remove();
    this.inkHost_ = null;
  }
}

class PluginController extends ContentController {
  /**
   * @param {HTMLEmbedElement} plugin
   * @param {PDFViewer} viewer
   * @param {Viewport} viewport
   */
  constructor(plugin, viewer, viewport) {
    super();
    this.plugin_ = plugin;
    this.viewer_ = viewer;
    this.viewport_ = viewport;

    /** @private {!Map<string, PromiseResolver>} */
    this.pendingTokens_ = new Map();
    this.plugin_.addEventListener(
        'message', e => this.handlePluginMessage_(e), false);
  }

  /**
   * Notify the plugin to stop reacting to scroll events while zoom is taking
   * place to avoid flickering.
   * @override
   */
  beforeZoom() {
    this.postMessage({type: 'stopScrolling'});

    if (this.viewport_.pinchPhase == Viewport.PinchPhase.PINCH_START) {
      const position = this.viewport_.position;
      const zoom = this.viewport_.zoom;
      const pinchPhase = this.viewport_.pinchPhase;
      this.postMessage({
        type: 'viewport',
        userInitiated: true,
        zoom: zoom,
        xOffset: position.x,
        yOffset: position.y,
        pinchPhase: pinchPhase
      });
    }
  }

  /**
   * Notify the plugin of the zoom change and to continue reacting to scroll
   * events.
   * @override
   */
  afterZoom() {
    const position = this.viewport_.position;
    const zoom = this.viewport_.zoom;
    const pinchVector = this.viewport_.pinchPanVector || {x: 0, y: 0};
    const pinchCenter = this.viewport_.pinchCenter || {x: 0, y: 0};
    const pinchPhase = this.viewport_.pinchPhase;

    this.postMessage({
      type: 'viewport',
      userInitiated: this.viewer_.isUserInitiatedEvent_,
      zoom: zoom,
      xOffset: position.x,
      yOffset: position.y,
      pinchPhase: pinchPhase,
      pinchX: pinchCenter.x,
      pinchY: pinchCenter.y,
      pinchVectorX: pinchVector.x,
      pinchVectorY: pinchVector.y
    });
  }

  // TODO(dstockwell): this method should be private, add controller APIs that
  // map to all of the existing usage. crbug.com/913279
  /**
   * Post a message to the PPAPI plugin. Some messages will cause an async reply
   * to be received through handlePluginMessage_().
   *
   * @param {Object} message Message to post.
   */
  postMessage(message) {
    this.plugin_.postMessage(message);
  }

  /** @override */
  rotateClockwise() {
    this.postMessage({type: 'rotateClockwise'});
  }

  /** @override */
  rotateCounterclockwise() {
    this.postMessage({type: 'rotateCounterclockwise'});
  }

  /** @override */
  print() {
    this.postMessage({type: 'print'});
  }

  /** @override */
  save(requireResult) {
    const resolver = new PromiseResolver();
    const newToken = createToken();
    this.pendingTokens_.set(newToken, resolver);
    this.postMessage({type: 'save', token: newToken, force: requireResult});
    return resolver.promise;
  }

  /** @override */
  async load(fileName, data) {
    const url = URL.createObjectURL(new Blob([data]));
    this.plugin_.removeAttribute('headers');
    this.plugin_.setAttribute('stream-url', url);
    this.plugin_.style.display = 'block';
    try {
      await this.viewer_.loaded;
    } finally {
      URL.revokeObjectURL(url);
    }
  }

  /** @override */
  unload() {
    this.plugin_.style.display = 'none';
  }

  /**
   * An event handler for handling message events received from the plugin.
   *
   * @param {MessageObject} message a message event.
   * @private
   */
  handlePluginMessage_(message) {
    switch (message.data.type.toString()) {
      case 'beep':
        this.viewer_.handleBeep();
        break;
      case 'documentDimensions':
        this.viewer_.setDocumentDimensions(message.data);
        break;
      case 'email':
        const href = 'mailto:' + message.data.to + '?cc=' + message.data.cc +
            '&bcc=' + message.data.bcc + '&subject=' + message.data.subject +
            '&body=' + message.data.body;
        window.location.href = href;
        break;
      case 'getPassword':
        this.viewer_.handlePasswordRequest();
        break;
      case 'getSelectedTextReply':
        this.viewer_.handleSelectedTextReply(message.data.selectedText);
        break;
      case 'goToPage':
        this.viewport_.goToPage(message.data.page);
        break;
      case 'loadProgress':
        this.viewer_.updateProgress(message.data.progress);
        break;
      case 'navigate':
        this.viewer_.handleNavigate(message.data.url, message.data.disposition);
        break;
      case 'printPreviewLoaded':
        this.viewer_.handlePrintPreviewLoaded();
        break;
      case 'setScrollPosition':
        this.viewport_.scrollTo(/** @type {!PartialPoint} */ (message.data));
        break;
      case 'scrollBy':
        this.viewport_.scrollBy(/** @type {!Point} */ (message.data));
        break;
      case 'metadata':
        this.viewer_.setDocumentMetadata(
            message.data.title, message.data.bookmarks,
            message.data.canSerializeDocument);
        break;
      case 'setIsSelecting':
        this.viewer_.setIsSelecting(message.data.isSelecting);
        break;
      case 'getNamedDestinationReply':
        this.viewer_.paramsParser_.onNamedDestinationReceived(
            message.data.pageNumber);
        break;
      case 'formFocusChange':
        this.viewer_.setIsFormFieldFocused(message.data.focused);
        break;
      case 'saveData':
        this.saveData_(message.data);
        break;
      case 'consumeSaveToken':
        const resolver = this.pendingTokens_.get(message.data.token);
        assert(this.pendingTokens_.delete(message.data.token));
        resolver.resolve(null);
        break;
    }
  }

  /**
   * Handles the pdf file buffer received from the plugin.
   *
   * @param {SaveDataMessageData} messageData data of the message event.
   * @private
   */
  saveData_(messageData) {
    assert(
        loadTimeData.getBoolean('pdfFormSaveEnabled') ||
        loadTimeData.getBoolean('pdfAnnotationsEnabled'));

    // Verify a token that was created by this instance is included to avoid
    // being spammed.
    const resolver = this.pendingTokens_.get(messageData.token);
    assert(this.pendingTokens_.delete(messageData.token));

    if (!messageData.dataToSave) {
      resolver.reject();
      return;
    }

    // Verify the file size and the first bytes to make sure it's a PDF. Cap at
    // 100 MB. This cap should be kept in sync with and is also enforced in
    // pdf/out_of_process_instance.cc.
    const MIN_FILE_SIZE = '%PDF1.0'.length;
    const MAX_FILE_SIZE = 100 * 1000 * 1000;

    const buffer = messageData.dataToSave;
    const bufView = new Uint8Array(buffer);
    assert(
        bufView.length <= MAX_FILE_SIZE,
        `File too large to be saved: ${bufView.length} bytes.`);
    assert(bufView.length >= MIN_FILE_SIZE);
    assert(
        String.fromCharCode(bufView[0], bufView[1], bufView[2], bufView[3]) ==
        '%PDF');

    resolver.resolve(messageData);
  }
}
