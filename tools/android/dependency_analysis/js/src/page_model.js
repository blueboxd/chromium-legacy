// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GraphModel, D3GraphData} from './graph_model.js';

/**
 * A container representing the visualization's node filter. Nodes included in
 * the filter are allowed to be displayed on the graph.
 */
class NodeFilterData {
  /**
   * Vue does not currently support reactivity on ES6 Sets. (Planned addition
   * for 3.0 https://github.com/vuejs/vue/issues/2410#issuecomment-434990853).
   * For performance, we maintain a Set for lookups when filtering nodes/edges
   * and expose an Array to the UI for reactivity. We sync the data in these
   * two structures manually.
   */
  constructor() {
    /** @public {!Set<string>} */
    this.nodeSet = new Set();
    /** @public {!Array<string>} */
    this.nodeList = [];
  }

  /**
   * Adds a node to the node set + array.
   * @param {string} nodeName The name of the node to add.
   */
  addNode(nodeName) {
    if (!this.nodeSet.has(nodeName)) {
      this.nodeSet.add(nodeName);
      this.nodeList.push(nodeName);
    }
  }

  /**
   * Removes a node from the node set + array.
   * @param {string} nodeName The name of the node to remove.
   */
  removeNode(nodeName) {
    const deleted = this.nodeSet.delete(nodeName);
    if (deleted) {
      const deleteIndex = this.nodeList.indexOf(nodeName);
      // TODO(yjlong): If order turns out to be unimportant, just swap the
      // last element and the deleted element, then pop.
      this.nodeList.splice(deleteIndex, 1);
    }
  }
}

/**
 * A container containing the page-wide state. This is the single source of
 * truth, parts of the container will be observed by Vue components on the page.
 */
class PageModel {
  /**
   * @param {!GraphModel} graphModel The graph data to visualize.
   */
  constructor(graphModel) {
    /** @private {!GraphModel} */
    this.graphModel_ = graphModel;

    /** @public {!NodeFilterData} */
    this.nodeFilterData = new NodeFilterData();

    /**
     * The data for the selected node details UI component.
     * @typedef {Object} SelectedNodeDetailsData
     * @property {?Node} selectedNode The selected node, if it exists.
     */
    /** @public {!SelectedNodeDetailsData} */
    this.selectedNodeDetailsData = {
      selectedNode: null,
    };

    /**
     * The data for the inbound depth UI component.
     * @typedef {Object} InboundDepthData
     * @property {number} inboundDepth The inbound depth.
     */
    /** @public {!InboundDepthData} */
    this.inboundDepthData = {
      inboundDepth: 0,
    };

    /**
     * The data for the outbound depth UI component.
     * @typedef {Object} OutboundDepthData
     * @property {number} outboundDepth The outbound depth.
     */
    /** @public {!OutboundDepthData} */
    this.outboundDepthData = {
      outboundDepth: 0,
    };
  }

  /**
   * @return {!D3GraphData} The nodes and edges to visualize.
   */
  getDataForD3() {
    return this.graphModel_.getDataForD3(
        this.nodeFilterData.nodeSet,
        this.inboundDepthData.inboundDepth,
        this.outboundDepthData.outboundDepth,
    );
  }

  /**
   * Gets the ids of all the nodes in the graph.
   * @return {!Array<string>} An array with the all node ids.
   */
  getNodeIds() {
    return [...this.graphModel_.nodes.keys()];
  }
}

export {
  NodeFilterData,
  PageModel,
};
