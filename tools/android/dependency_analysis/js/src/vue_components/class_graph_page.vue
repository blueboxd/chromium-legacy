<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="page-container">
    <div id="page-controls">
      <GraphFilterInput
          :node-ids="pageModel.getNodeIds()"
          :shorten-name="filterShortenName"
          @[CUSTOM_EVENTS.FILTER_SUBMITTED]="filterAddOrCheckNode"/>
      <GraphFilterItems
          :node-filter-data="displaySettingsData.nodeFilterData"
          :shorten-name="filterShortenName"
          @[CUSTOM_EVENTS.FILTER_REMOVE]="filterRemoveNode"
          @[CUSTOM_EVENTS.FILTER_CHECK_ALL]="filterCheckAll"
          @[CUSTOM_EVENTS.FILTER_UNCHECK_ALL]="filterUncheckAll"/>
      <NumericInput
          description="Change inbound (blue) depth:"
          input-id="inbound-input"
          :input-value.sync="displaySettingsData.inboundDepth"/>
      <NumericInput
          description="Change outbound (yellow) depth:"
          input-id="outbound-input"
          :input-value.sync="displaySettingsData.outboundDepth"/>
    </div>
    <div id="graph-and-node-details-container">
      <GraphVisualization
          :graph-update-triggers="[
            getNodeGroup,
            displaySettingsData,
          ]"
          :page-model="pageModel"
          :display-settings-data="displaySettingsData"
          :get-node-group="getNodeGroup"
          @[CUSTOM_EVENTS.NODE_CLICKED]="graphNodeClicked"
          @[CUSTOM_EVENTS.NODE_DOUBLE_CLICKED]="graphNodeDoubleClicked"/>
      <div id="node-details-container">
        <GraphDisplaySettings
            :display-settings-data="displaySettingsData"/>
        <ClassGraphHullSettings
            :selected-hull-display.sync="displaySettingsData.hullDisplay"/>
        <GraphSelectedNodeDetails
            :selected-node-details-data="pageModel.selectedNodeDetailsData"
            @[CUSTOM_EVENTS.DETAILS_CHECK_NODE]="filterAddOrCheckNode"
            @[CUSTOM_EVENTS.DETAILS_UNCHECK_NODE]="filterUncheckNode"/>
        <ClassDetailsPanel
            :selected-class="pageModel.selectedNodeDetailsData.selectedNode"/>
      </div>
    </div>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';
import {HullDisplay} from '../class_view_consts.js';
import {PagePathName, UrlProcessor} from '../url_processor.js';

import {ClassNode, GraphNode} from '../graph_model.js';
import {PageModel} from '../page_model.js';
import {ClassDisplaySettingsData} from '../display_settings_data.js';
import {parseClassGraphModelFromJson} from '../process_graph_json.js';
import {shortenClassNameWithPackage} from '../chrome_hooks.js';

import ClassDetailsPanel from './class_details_panel.vue';
import ClassGraphHullSettings from './class_graph_hull_settings.vue';
import GraphDisplaySettings from './graph_display_settings.vue';
import GraphFilterInput from './graph_filter_input.vue';
import GraphFilterItems from './graph_filter_items.vue';
import GraphSelectedNodeDetails from './graph_selected_node_details.vue';
import GraphVisualization from './graph_visualization.vue';
import NumericInput from './numeric_input.vue';

/**
 * @param {!ClassNode} node The node to get the build target of.
 * @return {?string} The build target of the node.
 */
function getNodeBuildTarget(node) {
  if (node.buildTargets.length > 0) {
    // A few classes have multiple targets, just take the first one.
    return node.buildTargets[0];
  }
  return null;
}

// @vue/component
const ClassGraphPage = {
  components: {
    ClassDetailsPanel,
    ClassGraphHullSettings,
    GraphDisplaySettings,
    GraphFilterInput,
    GraphFilterItems,
    GraphSelectedNodeDetails,
    GraphVisualization,
    NumericInput,
  },
  props: {
    graphJson: Object,
  },

  /**
   * Various references to objects used across the entire class page.
   * @typedef {Object} ClassPageData
   * @property {PageModel} pageModel The data store for the page.
   * @property {!ClassDisplaySettingsData} displaySettingsData Additional data
   *   store for the graph's display settings.
   * @property {PagePathName} pagePathName The pathname for the page.
   */

  /**
   * @return {ClassPageData} The objects used throughout the page.
   */
  data: function() {
    const graphModel = parseClassGraphModelFromJson(this.graphJson);
    const pageModel = new PageModel(graphModel);
    const displaySettingsData = new ClassDisplaySettingsData();

    return {
      pageModel,
      displaySettingsData,
      pagePathName: PagePathName.CLASS,
    };
  },
  computed: {
    CUSTOM_EVENTS: () => CUSTOM_EVENTS,
    getNodeGroup: function() {
      switch (this.displaySettingsData.hullDisplay) {
        case HullDisplay.BUILD_TARGET:
          return getNodeBuildTarget;
        default:
          return () => null;
      }
    },
  },
  watch: {
    displaySettingsData: {
      handler: function() {
        this.updateDocumentUrl();
      },
      deep: true,
    },
  },
  /**
   * Parses out data from the current URL to initialize the visualization with.
   */
  mounted: function() {
    const pageUrl = new URL(document.URL);
    const pageUrlProcessor = new UrlProcessor(pageUrl.searchParams);
    this.displaySettingsData.readUrlProcessor(pageUrlProcessor);

    if (this.displaySettingsData.nodeFilterData.filterList.length === 0) {
      // TODO(yjlong): This is test data. Remove this when no longer needed.
      [
        'org.chromium.chrome.browser.tabmodel.AsyncTabParams',
        'org.chromium.chrome.browser.ActivityTabProvider',
        'org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver',
      ].forEach(nodeName => this.filterAddOrCheckNode(nodeName));
    }
  },
  methods: {
    updateDocumentUrl() {
      const urlProcessor = UrlProcessor.createForOutput();
      this.displaySettingsData.updateUrlProcessor(urlProcessor);

      const pageUrl = urlProcessor.getUrl(document.URL, PagePathName.CLASS);
      history.replaceState(null, '', pageUrl);
    },
    filterShortenName: shortenClassNameWithPackage,
    filterRemoveNode: function(nodeName) {
      this.displaySettingsData.nodeFilterData.removeNode(nodeName);
    },
    filterAddOrCheckNode: function(nodeName) {
      this.displaySettingsData.nodeFilterData.addOrFindNode(
          nodeName).checked = true;
    },
    filterUncheckNode: function(nodeName) {
      this.displaySettingsData.nodeFilterData.addOrFindNode(
          nodeName).checked = false;
    },
    filterCheckAll: function() {
      this.displaySettingsData.nodeFilterData.checkAll();
    },
    filterUncheckAll: function() {
      this.displaySettingsData.nodeFilterData.uncheckAll();
    },
    /**
     * @param {number} depth The new inbound depth.
     */
    setInboundDepth: function(depth) {
      this.displaySettingsData.inboundDepth = depth;
    },
    /**
     * @param {number} depth The new outbound depth.
     */
    setOutboundDepth: function(depth) {
      this.displaySettingsData.outboundDepth = depth;
    },
    /**
     * @param {?GraphNode} node The selected node. May be `null`, which will
     *     reset the selection to the state with no node.
     */
    graphNodeClicked: function(node) {
      this.pageModel.selectedNodeDetailsData.selectedNode = node;
    },
    /**
     * @param {!GraphNode} node The double-clicked node.
     */
    graphNodeDoubleClicked: function(node) {
      if (node.visualizationState.selectedByFilter) {
        this.filterUncheckNode(node.id);
      } else {
        this.filterAddOrCheckNode(node.id);
      }
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
