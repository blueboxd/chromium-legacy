// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HullDisplay} from './class_view_consts.js';
import {UrlProcessor, URL_PARAM_KEYS} from './url_processor.js';

/**
 * Various different graph edge color schemes.
 * @enum {string}
 */
const GraphEdgeColor = {
  DEFAULT: 'default',
  GREY_GRADIENT: 'grey-gradient',
  BLUE_TO_RED: 'blue-to-red',
};

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

/** Data store containing graph display-related settings. */
class DisplaySettingsData {
  /** Sets up default values for display settings. */
  constructor() {
    /** @public {!NodeFilterData} */
    this.nodeFilterData = new NodeFilterData();
    /** @public {number} */
    this.inboundDepth = 0;
    /** @public {number} */
    this.outboundDepth = 1;
    /** @public {boolean} */
    this.curveEdges = true;
    /** @public {boolean} */
    this.colorOnlyOnHover = false;
    /** @public {string} */
    this.graphEdgeColor = GraphEdgeColor.GREY_GRADIENT;
  }

  /**
   * Updates a UrlProcessor with all contained data.
   * @param {!UrlProcessor} urlProcessor The UrlProcessor to update.
   */
  updateUrlProcessor(urlProcessor) {
    urlProcessor.append(URL_PARAM_KEYS.INBOUND_DEPTH, this.inboundDepth);
    urlProcessor.append(URL_PARAM_KEYS.OUTBOUND_DEPTH, this.outboundDepth);
    urlProcessor.append(URL_PARAM_KEYS.CURVE_EDGES, this.curveEdges);
    urlProcessor.append(
        URL_PARAM_KEYS.COLOR_ONLY_ON_HOVER, this.colorOnlyOnHover);
    urlProcessor.append(URL_PARAM_KEYS.EDGE_COLOR, this.graphEdgeColor);
    if (this.nodeFilterData.nodeList.length > 0) {
      urlProcessor.appendArray(
          URL_PARAM_KEYS.FILTER, this.nodeFilterData.nodeList);
    }
  }

  /**
   * Reads all contained data from a UrlProcessor.
   * @param {!UrlProcessor} urlProcessor The UrlProcessor to read from.
   */
  readUrlProcessor(urlProcessor) {
    this.inboundDepth = urlProcessor.getInt(
        URL_PARAM_KEYS.INBOUND_DEPTH, this.inboundDepth);
    this.outboundDepth = urlProcessor.getInt(
        URL_PARAM_KEYS.OUTBOUND_DEPTH, this.outboundDepth);
    this.curveEdges = urlProcessor.getBool(
        URL_PARAM_KEYS.CURVE_EDGES, this.curveEdges);
    this.colorOnlyOnHover = urlProcessor.getBool(
        URL_PARAM_KEYS.COLOR_ONLY_ON_HOVER, this.colorOnlyOnHover);
    this.graphEdgeColor = urlProcessor.getString(
        URL_PARAM_KEYS.EDGE_COLOR, this.graphEdgeColor);
    for (const filterItem of urlProcessor.getArray(URL_PARAM_KEYS.FILTER, [])) {
      this.nodeFilterData.addNode(filterItem);
    }
  }
}

/** Data store containing class graph display-related settings. */
class ClassDisplaySettingsData extends DisplaySettingsData {
  /** Sets up default values for display settings. */
  constructor() {
    super();
    /** @public {string} */
    this.hullDisplay = HullDisplay.BUILD_TARGET;
  }

  /**
   * Updates a UrlProcessor with all contained data.
   * @param {!UrlProcessor} urlProcessor The UrlProcessor to update.
   */
  updateUrlProcessor(urlProcessor) {
    super.updateUrlProcessor(urlProcessor);
    urlProcessor.append(URL_PARAM_KEYS.HULL_DISPLAY, this.hullDisplay);
  }

  /**
   * Reads all contained data from a UrlProcessor.
   * @param {!UrlProcessor} urlProcessor The UrlProcessor to read from.
   */
  readUrlProcessor(urlProcessor) {
    super.readUrlProcessor(urlProcessor);
    this.hullDisplay = urlProcessor.getString(
        URL_PARAM_KEYS.HULL_DISPLAY, this.hullDisplay);
  }
}

/** Data store containing package graph display-related settings. */
class PackageDisplaySettingsData extends DisplaySettingsData {}

export {
  GraphEdgeColor,
  ClassDisplaySettingsData,
  PackageDisplaySettingsData,
};
