<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="page-container">
    <div id="page-controls">
      <GraphFilterInput
          :node-ids="pageModel.getNodeIds()"
          :nodes-already-in-filter="
            displaySettingsData.nodeFilterData.filterList"
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
          :input-value.sync="displaySettingsData.inboundDepth"
          :min-value="0"/>
      <NumericInput
          description="Change outbound (yellow) depth:"
          input-id="outbound-input"
          :input-value.sync="displaySettingsData.outboundDepth"
          :min-value="0"/>
    </div>
    <div id="graph-and-node-details-container">
      <GraphVisualization
          :graph-update-triggers="[
            displaySettingsData,
          ]"
          :page-model="pageModel"
          :display-settings-data="displaySettingsData"
          @[CUSTOM_EVENTS.NODE_CLICKED]="graphNodeClicked"
          @[CUSTOM_EVENTS.NODE_DOUBLE_CLICKED]="graphNodeDoubleClicked"/>
      <div id="node-details-container">
        <GraphDisplaySettings
            :display-settings-data="displaySettingsData"/>
        <GraphSelectedNodeDetails
            :selected-node-details-data="pageModel.selectedNodeDetailsData"
            @[CUSTOM_EVENTS.DETAILS_CHECK_NODE]="filterAddOrCheckNode"
            @[CUSTOM_EVENTS.DETAILS_UNCHECK_NODE]="filterUncheckNode"/>
        <PackageDetailsPanel
            :selected-package="pageModel.selectedNodeDetailsData.selectedNode"/>
      </div>
    </div>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';
import {PagePathName, UrlProcessor} from '../url_processor.js';

import {GraphNode} from '../graph_model.js';
import {PageModel} from '../page_model.js';
import {PackageDisplaySettingsData} from '../display_settings_data.js';
import {parsePackageGraphModelFromJson} from '../process_graph_json.js';
import {shortenPackageName} from '../chrome_hooks.js';

import GraphDisplaySettings from './graph_display_settings.vue';
import GraphFilterInput from './graph_filter_input.vue';
import GraphFilterItems from './graph_filter_items.vue';
import GraphSelectedNodeDetails from './graph_selected_node_details.vue';
import GraphVisualization from './graph_visualization.vue';
import NumericInput from './numeric_input.vue';
import PackageDetailsPanel from './package_details_panel.vue';

// @vue/component
const PackageGraphPage = {
  components: {
    GraphDisplaySettings,
    GraphFilterInput,
    GraphFilterItems,
    GraphSelectedNodeDetails,
    GraphVisualization,
    NumericInput,
    PackageDetailsPanel,
  },
  props: {
    graphJson: Object,
  },

  /**
   * Various references to objects used across the entire package page.
   * @typedef {Object} PackagePageData
   * @property {!PageModel} pageModel The data store for the page.
   * @property {!PackageDisplaySettingsData} displaySettingsData Additional data
   *   store for the graph's display settings.
   * @property {PagePathName} pagePathName The pathname for the page.
   */

  /**
   * @return {PackagePageData} The objects used throughout the page.
  */
  data: function() {
    const graphModel = parsePackageGraphModelFromJson(this.graphJson);
    const pageModel = new PageModel(graphModel);
    const displaySettingsData = new PackageDisplaySettingsData();

    return {
      pageModel,
      displaySettingsData,
      pagePathName: PagePathName.PACKAGE,
    };
  },
  computed: {
    CUSTOM_EVENTS: () => CUSTOM_EVENTS,
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
        'org.chromium.base',
        'org.chromium.chrome.browser.gsa',
        'org.chromium.chrome.browser.omaha',
        'org.chromium.chrome.browser.media',
        'org.chromium.ui.base',
      ].forEach(nodeName => this.filterAddOrCheckNode(nodeName));
    }
  },
  methods: {
    updateDocumentUrl() {
      const urlProcessor = UrlProcessor.createForOutput();
      this.displaySettingsData.updateUrlProcessor(urlProcessor);

      const pageUrl = urlProcessor.getUrl(document.URL, PagePathName.PACKAGE);
      history.replaceState(null, '', pageUrl);
    },
    filterShortenName: shortenPackageName,
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
