// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CUSTOM_EVENTS} from '../vue_custom_events.js';

import {GraphFilterInput} from './graph_filter_input.js';
import {GraphFilterItems} from './graph_filter_items.js';
import {GraphInboundInput} from './graph_inbound_input.js';
import {GraphOutboundInput} from './graph_outbound_input.js';
import {GraphSelectedNodeDetails} from './graph_selected_node_details.js';
import {GraphVisualization} from './graph_visualization.js';
import {PackageDetailsPanel} from './package_details_panel.js';
import {PageUrlGenerator} from './page_url_generator.js';

import {generateFilterFromUrl} from '../url_processor.js';
import {GraphNode} from '../graph_model.js';
import {PageModel} from '../page_model.js';
import {PagePathName} from '../url_processor.js';
import {parsePackageGraphModelFromJson} from '../process_graph_json.js';

const PackageGraphPage = Vue.component('package-graph-page', {
  components: {
    'graph-filter-input': GraphFilterInput,
    'graph-filter-items': GraphFilterItems,
    'graph-inbound-input': GraphInboundInput,
    'graph-outbound-input': GraphOutboundInput,
    'graph-selected-node-details': GraphSelectedNodeDetails,
    'graph-visualization': GraphVisualization,
    'package-details-panel': PackageDetailsPanel,
    'page-url-generator': PageUrlGenerator,
  },
  props: ['graphJson'],

  /**
   * Various references to objects used across the entire package page.
   * @typedef {Object} PackagePageData
   * @property {PageModel} pageModel The data store for the page.
   * @property {PagePathName} pagePathName The pathname for the page.
   * @property {number} graphDataUpdateTicker Incremented every time we want to
   *     trigger a visualization update. See graph_visualization.js for further
   *     explanation on this variable.
   */

  /**
   * @return {PackagePageData} The objects used throughout the page.
  */
  data: function() {
    const graphModel = parsePackageGraphModelFromJson(this.graphJson);
    const pageModel = new PageModel(graphModel);

    return {
      pageModel,
      pagePathName: PagePathName.PACKAGE,
      graphDataUpdateTicker: 0,
    };
  },
  /**
   * Parses out data from the current URL to initialize the visualization with.
   */
  mounted: function() {
    const includedNodesInUrl = generateFilterFromUrl(document.URL);

    if (includedNodesInUrl.length !== 0) {
      this.addNodesToFilter(includedNodesInUrl);
    } else {
      // TODO(yjlong): This is test data. Remove this when no longer needed.
      this.addNodesToFilter([
        'org.chromium.base',
        'org.chromium.chrome.browser.gsa',
        'org.chromium.chrome.browser.omaha',
        'org.chromium.chrome.browser.media',
        'org.chromium.ui.base',
      ]);
    }

    this.setOutboundDepth(1);
    this.graphDataUpdateTicker++;
  },
  methods: {
    /**
     * @param {string} nodeName The node to add.
     */
    addNodeToFilter: function(nodeName) {
      this.pageModel.nodeFilterData.addNode(nodeName);
      this.graphDataUpdateTicker++;
    },
    /**
     * Adds all supplied nodes to the node filter, then increments
     * `graphDataUpdateTicker` once at the end, even if `nodeNames` is empty.
     * @param {!Array<string>} nodeNames The nodes to add.
     */
    addNodesToFilter: function(nodeNames) {
      for (const nodeName of nodeNames) {
        this.pageModel.nodeFilterData.addNode(nodeName);
      }
      this.graphDataUpdateTicker++;
    },
    /**
     * @param {string} nodeName The node to remove.
     */
    removeNodeFromFilter: function(nodeName) {
      this.pageModel.nodeFilterData.removeNode(nodeName);
      this.graphDataUpdateTicker++;
    },
    /**
     * @param {number} depth The new inbound depth.
     */
    setInboundDepth: function(depth) {
      this.pageModel.inboundDepthData.inboundDepth = depth;
      this.graphDataUpdateTicker++;
    },
    /**
     * @param {number} depth The new outbound depth.
     */
    setOutboundDepth: function(depth) {
      this.pageModel.outboundDepthData.outboundDepth = depth;
      this.graphDataUpdateTicker++;
    },
    /**
     * @param {?GraphNode} node The selected node. May be `null`, which will
     *     reset the selection to the state with no node.
     */
    graphNodeClicked: function(node) {
      this.pageModel.selectedNodeDetailsData.selectedNode = node;
    },
  },
  template: `
    <div id="page-container">
      <div id="page-controls">
        <graph-filter-input
          :node-ids="this.pageModel.getNodeIds()"
          @${CUSTOM_EVENTS.FILTER_SUBMITTED}="this.addNodeToFilter"
        ></graph-filter-input>
        <graph-filter-items
          :node-filter-data="this.pageModel.nodeFilterData"
          @${CUSTOM_EVENTS.FILTER_ELEMENT_CLICKED}="this.removeNodeFromFilter"
        ></graph-filter-items>
        <graph-inbound-input
          :inbound-depth-data="this.pageModel.inboundDepthData"
          @${CUSTOM_EVENTS.INBOUND_DEPTH_UPDATED}="this.setInboundDepth"
        ></graph-inbound-input>
        <graph-outbound-input
          :outbound-depth-data="this.pageModel.outboundDepthData"
          @${CUSTOM_EVENTS.OUTBOUND_DEPTH_UPDATED}="this.setOutboundDepth"
        ></graph-outbound-input>
      </div>
      <div id="graph-and-node-details-container">
        <graph-visualization
          :graph-data-update-ticker="this.graphDataUpdateTicker"
          :page-model="this.pageModel"
          @${CUSTOM_EVENTS.NODE_CLICKED}="graphNodeClicked"
        ></graph-visualization>
        <div id="node-details-container">
          <graph-selected-node-details
            :selected-node-details-data="this.pageModel.selectedNodeDetailsData"
            @${CUSTOM_EVENTS.ADD_TO_FILTER_CLICKED}="addNodeToFilter"
            @${CUSTOM_EVENTS.REMOVE_FROM_FILTER_CLICKED}="removeNodeFromFilter"
          ></graph-selected-node-details>
          <package-details-panel
            :selected-package=
              "this.pageModel.selectedNodeDetailsData.selectedNode"
          ></package-details-panel>
        </div>
      </div>
      <page-url-generator
        :page-path-name="this.pagePathName"
        :node-filter-data="this.pageModel.nodeFilterData"
      ></page-url-generator>
    </div>`,
});

export {
  PackageGraphPage,
};
