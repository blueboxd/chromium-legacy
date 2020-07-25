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
          :inbound-depth-data="pageModel.inboundDepthData"/>
      <GraphOutboundInput
          :outbound-depth-data="pageModel.outboundDepthData"/>
    </div>
    <div id="graph-and-node-details-container">
      <GraphVisualization
          :graph-update-triggers="graphUpdateTriggers"
          :page-model="pageModel"
          @[CUSTOM_EVENTS.NODE_CLICKED]="graphNodeClicked"/>
      <div id="node-details-container">
        <GraphDisplaySettings
            :display-settings-data="pageModel.displaySettingsData"/>
        <GraphSelectedNodeDetails
            :selected-node-details-data="pageModel.selectedNodeDetailsData"
            @[CUSTOM_EVENTS.ADD_TO_FILTER_CLICKED]="addNodeToFilter"
            @[CUSTOM_EVENTS.REMOVE_FROM_FILTER_CLICKED]="removeNodeFromFilter"/>
        <PackageDetailsPanel
            :selected-package="pageModel.selectedNodeDetailsData.selectedNode"/>
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
import {parsePackageGraphModelFromJson} from '../process_graph_json.js';

import GraphDisplaySettings from './graph_display_settings.vue';
import GraphFilterInput from './graph_filter_input.vue';
import GraphFilterItems from './graph_filter_items.vue';
import GraphInboundInput from './graph_inbound_input.vue';
import GraphOutboundInput from './graph_outbound_input.vue';
import GraphSelectedNodeDetails from './graph_selected_node_details.vue';
import GraphVisualization from './graph_visualization.vue';
import PackageDetailsPanel from './package_details_panel.vue';
import PageUrlGenerator from './page_url_generator.vue';

// @vue/component
const PackageGraphPage = {
  components: {
    GraphDisplaySettings,
    GraphFilterInput,
    GraphFilterItems,
    GraphInboundInput,
    GraphOutboundInput,
    GraphSelectedNodeDetails,
    GraphVisualization,
    PackageDetailsPanel,
    PageUrlGenerator,
  },
  props: {
    graphJson: Object,
  },

  /**
   * Various references to objects used across the entire package page.
   * @typedef {Object} PackagePageData
   * @property {PageModel} pageModel The data store for the page.
   * @property {PagePathName} pagePathName The pathname for the page.
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
    };
  },
  computed: {
    CUSTOM_EVENTS: () => CUSTOM_EVENTS,
    graphUpdateTriggers: function() {
      return [
        this.pageModel.displaySettingsData,
        this.pageModel.nodeFilterData.nodeList,
        this.pageModel.inboundDepthData.inboundDepth,
        this.pageModel.outboundDepthData.outboundDepth,
      ];
    },
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
    },
    /**
     * @param {string} nodeName The node to remove.
     */
    removeNodeFromFilter: function(nodeName) {
      this.pageModel.nodeFilterData.removeNode(nodeName);
    },
    /**
     * @param {number} depth The new inbound depth.
     */
    setInboundDepth: function(depth) {
      this.pageModel.inboundDepthData.inboundDepth = depth;
    },
    /**
     * @param {number} depth The new outbound depth.
     */
    setOutboundDepth: function(depth) {
      this.pageModel.outboundDepthData.outboundDepth = depth;
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

export default PackageGraphPage;
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

#node-details-container {
  display: flex;
  flex-direction: column;
}
</style>
