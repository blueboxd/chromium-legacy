// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('graph_tab', function() {
  'use strict';
  /**
   * @implements {performanceManager.mojom.WebUIGraphChangeStreamInterface}
   */
  class WebUIGraphChangeStreamImpl {
    constructor(contentWindow) {
      this.contentWindow_ = contentWindow;
    }

    /**
     * @param {string} type
     * @param {Object|number} data
     */
    postMessage_(type, data) {
      this.contentWindow_.postMessage([type, data], '*');
    }

    /** @override */
    frameCreated(frame) {
      this.postMessage_('frameCreated', frame);
    }

    /** @override */
    pageCreated(page) {
      this.postMessage_('pageCreated', page);
    }

    /** @override */
    processCreated(process) {
      this.postMessage_('processCreated', process);
    }

    /** @override */
    frameChanged(frame) {
      this.postMessage_('frameChanged', frame);
    }

    /** @override */
    pageChanged(page) {
      this.postMessage_('pageChanged', page);
    }

    /** @override */
    processChanged(process) {
      this.postMessage_('processChanged', process);
    }

    /** @override */
    favIconDataAvailable(icon_info) {
      this.postMessage_('favIconDataAvailable', icon_info);
    }

    /** @override */
    nodeDeleted(nodeId) {
      this.postMessage_('nodeDeleted', nodeId);
    }
  }

  return {
    WebUIGraphChangeStreamImpl: WebUIGraphChangeStreamImpl,
  };
});

Polymer({
  is: 'graph-tab',

  /**
   * The Mojo graph data source.
   *
   * @private {performanceManager.mojom.WebUIGraphDumpRemote}
   */
  graphDump_: null,

  /**
   * The graph change listener.
   *
   * @private {performanceManager.mojom.WebUIGraphChangeStreamInterface}
   */
  changeListener_: null,

  /** @override */
  ready: function() {
    this.graphDump_ = performanceManager.mojom.WebUIGraphDump.getRemote();
  },

  /** @override */
  detached: function() {
    // TODO(siggi): Is there a way to tear down the binding explicitly?
    this.graphDump_ = null;
    this.changeListener_ = null;
  },

  /** @private */
  onWebViewReady_: function() {
    this.changeListener_ =
        new graph_tab.WebUIGraphChangeStreamImpl(this.$.webView.contentWindow);
    this.client_ = new performanceManager.mojom.WebUIGraphChangeStreamReceiver(
        this.changeListener_);
    // Save helper to work around closure compiler bug: https://crbug.com/969212
    const helper = this.client_.$;

    // Subscribe for graph updates.
    this.graphDump_.subscribeToChanges(helper.bindNewPipeAndPassRemote());
  },
});
