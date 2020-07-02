// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageModel} from './page_model.js';
import {GraphView} from './graph_view.js';
import {Node} from './graph_model.js';

/** Controller class for page-wide events and communication. */
class PageController {
  /**
   * @param {!PageModel} pageModel The model holding all page-related state.
   * @param {!GraphView} graphView The view which renders the visualization.
   */
  constructor(pageModel, graphView) {
    /** @private {!PageModel} */
    this.pageModel_ = pageModel;
    /** @private {!GraphView} */
    this.graphView_ = graphView;

    this.graphView_.registerOnNodeClicked(node => {
      this.pageModel_.selectedNodeDetailsData.selectedNode = node;
    });
  }

  /**
   * Adds all supplied nodes to the node filter, then calls `updateGraphData`
   * once at the very end, even if `nodeNames` is empty.
   * @param {!Array<string>} nodeNames The nodes to add.
   */
  addIncludedNodes(nodeNames) {
    for (const nodeName of nodeNames) {
      this.pageModel_.nodeFilterData.addNode(nodeName);
    }
    this.graphView_.updateGraphData(this.pageModel_.getDataForD3());
  }

  /**
   * @param {string} nodeName The node to remove.
   */
  removeIncludedNode(nodeName) {
    this.pageModel_.nodeFilterData.removeNode(nodeName);
    this.graphView_.updateGraphData(this.pageModel_.getDataForD3());
  }

  /**
   * @param {number} depth The new inbound depth.
   */
  setInboundDepth(depth) {
    this.pageModel_.inboundDepthData.inboundDepth = depth;
    this.graphView_.updateGraphData(this.pageModel_.getDataForD3());
  }

  /**
   * @param {number} depth The new outbound depth.
   */
  setOutboundDepth(depth) {
    this.pageModel_.outboundDepthData.outboundDepth = depth;
    this.graphView_.updateGraphData(this.pageModel_.getDataForD3());
  }
}

export {
  PageController,
};
