// Copyright 2019 The Chromium Authors. All rights reserved.
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
      const zoom = this.viewport_.getZoom();
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
    const zoom = this.viewport_.getZoom();
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
