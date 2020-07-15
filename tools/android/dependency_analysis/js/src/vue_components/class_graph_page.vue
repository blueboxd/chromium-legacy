<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="page-container">
    <div id="page-controls">
      <GraphFilterInput
          :node-ids="pageModel.getNodeIds()"
          @[CUSTOM_EVENTS.FILTER_SUBMITTED]="addNodeToFilter"/>
      <GraphFilterItems
          :node-filter-data="pageModel.nodeFilterData"
          @[CUSTOM_EVENTS.FILTER_ELEMENT_CLICKED]="removeNodeFromFilter"/>
      <GraphInboundInput
          :inbound-depth-data="pageModel.inboundDepthData"
          @[CUSTOM_EVENTS.INBOUND_DEPTH_UPDATED]="setInboundDepth"/>
      <GraphOutboundInput
          :outbound-depth-data="pageModel.outboundDepthData"
          @[CUSTOM_EVENTS.OUTBOUND_DEPTH_UPDATED]="setOutboundDepth"/>
    </div>
    <div id="graph-and-node-details-container">
      <GraphVisualization
          :graph-data-update-ticker="graphDataUpdateTicker"
          :page-model="pageModel"
          @[CUSTOM_EVENTS.NODE_CLICKED]="graphNodeClicked"/>
      <div id="node-details-container">
        <GraphSelectedNodeDetails
            :selected-node-details-data="pageModel.selectedNodeDetailsData"
            @[CUSTOM_EVENTS.ADD_TO_FILTER_CLICKED]="addNodeToFilter"
            @[CUSTOM_EVENTS.REMOVE_FROM_FILTER_CLICKED]="removeNodeFromFilter"/>
        <LinkToGraph
            v-if="pageModel.selectedNodeDetailsData.selectedNode !== null"
            :filter="
              [pageModel.selectedNodeDetailsData.selectedNode.packageName]"
            :graph-type="PagePathName.PACKAGE"
            :text="'View ' +
              pageModel.selectedNodeDetailsData.selectedNode.packageName"/>
      </div>
    </div>
    <PageUrlGenerator
        :page-path-name="pagePathName"
        :node-filter-data="pageModel.nodeFilterData"/>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';
import {PagePathName, generateFilterFromUrl} from '../url_processor.js';

import {GraphNode} from '../graph_model.js';
import {PageModel} from '../page_model.js';
import {parseClassGraphModelFromJson} from '../process_graph_json.js';

import GraphFilterInput from './graph_filter_input.vue';
import GraphFilterItems from './graph_filter_items.vue';
import GraphInboundInput from './graph_inbound_input.vue';
import GraphOutboundInput from './graph_outbound_input.vue';
import GraphSelectedNodeDetails from './graph_selected_node_details.vue';
import GraphVisualization from './graph_visualization.vue';
import LinkToGraph from './link_to_graph.vue';
import PageUrlGenerator from './page_url_generator.vue';

const ClassGraphPage = {
  components: {
    GraphFilterInput,
    GraphFilterItems,
    GraphInboundInput,
    GraphOutboundInput,
    GraphSelectedNodeDetails,
    GraphVisualization,
    LinkToGraph,
    PageUrlGenerator,
  },
  props: {
    graphJson: Object,
  },

  /**
   * Various references to objects used across the entire class page.
   * @typedef {Object} ClassPageData
   * @property {PageModel} pageModel The data store for the page.
   * @property {PagePathName} pagePathName The pathname for the page.
   * @property {number} graphDataUpdateTicker Incremented every time we want to
   *     trigger a visualization update. See graph_visualization.js for further
   *     explanation on this variable.
   */

  /**
   * @return {ClassPageData} The objects used throughout the page.
   */
  data: function() {
    const graphModel = parseClassGraphModelFromJson(this.graphJson);
    const pageModel = new PageModel(graphModel);

    return {
      pageModel,
      pagePathName: PagePathName.CLASS,
      graphDataUpdateTicker: 0,
    };
  },
  computed: {
    CUSTOM_EVENTS: () => CUSTOM_EVENTS,
    PagePathName: () => PagePathName,
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
        'org.chromium.chrome.browser.tabmodel.AsyncTabParams',
        'org.chromium.chrome.browser.ActivityTabProvider',
        'org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver',
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
};

export default ClassGraphPage;
</script>

<style>
.user-input-group {
  display: flex;
  flex-direction: column;
}
</style>

<style scoped>
#page-container {
  display: flex;
  flex-direction: column;
}

#page-controls {
  display: flex;
  flex-direction: row;
  height: 15vh;
}

#graph-and-node-details-container {
  display: flex;
  flex-direction: row;
}
</style>
