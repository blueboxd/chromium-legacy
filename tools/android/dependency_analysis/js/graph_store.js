// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Shared state storage for runtime graph configuration. Based on
 * the store pattern in (https://vuejs.org/v2/guide/state-management.html).
 *
 * TODO(yjlong): I'm not really sure if this is a good way to do things (global
 * store accessible anywhere). Once more user config is available, revisit this
 * and see if there is need for an alternative.
 */

/**
 * A global store for graph state data.
 *
 * An instance of this will be exported and available anywhere via import. The
 * `state` property will be included in the `data` of UI controls so that
 * changes to the state are synced with the UI.
 */
class GraphStore {
  /**
   * Vue does not currently support reactivity on ES6 Sets. (Planned addition
   * for 3.0 https://github.com/vuejs/vue/issues/2410#issuecomment-434990853).
   * For performance, we maintain a Set for lookups when filtering nodes/edges
   * and expose an Array to the UI for reactivity. We sync the data in these
   * two structures manually.
   */
  constructor() {
    /** @public {!Set<string>} */
    this.includedNodeSet = new Set();
    // TODO(yjlong): Holding off on annotating this until we have a better idea
    // of what the state will look like.
    this.state = {
      includedNodes: [],
      inboundDepth: 0,
      outboundDepth: 0,
    };
  }

  /**
   * Adds a node to the included node set + array.
   * @param {string} nodeName The name of the node to add.
   */
  addIncludedNode(nodeName) {
    if (!this.includedNodeSet.has(nodeName)) {
      this.includedNodeSet.add(nodeName);
      this.state.includedNodes.push(nodeName);
    }
  }

  /**
   * Removes a node from the included node set + array.
   * @param {string} nodeName The name of the node to remove.
   */
  removeIncludedNode(nodeName) {
    const deleted = this.includedNodeSet.delete(nodeName);
    if (deleted) {
      const deleteIndex = this.state.includedNodes.indexOf(nodeName);
      // TODO(yjlong): If order turns out to be unimportant, just swap the last
      // element and the deleted element, then pop.
      this.state.includedNodes.splice(deleteIndex, 1);
    }
  }

  setInboundDepth(depth) {
    this.state.inboundDepth = depth;
  }

  setOutboundDepth(depth) {
    this.state.outboundDepth = depth;
  }
}

GraphStore.instance = new GraphStore();

export {
  GraphStore,
};
