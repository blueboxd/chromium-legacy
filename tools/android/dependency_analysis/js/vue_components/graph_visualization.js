// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CUSTOM_EVENTS} from '../vue_custom_events.js';
import {GraphView} from '../graph_view.js';

const GraphVisualization = {
  props: ['graphDataUpdateTicker', 'pageModel'],
  /**
   * Initializes the `GraphView` backing this visualization component. It's
   * important we initialize on `mounted`, since GraphView's constructor binds
   * to a DOM element which must exist at the time of construction.
   */
  mounted: function() {
    this.graphView = new GraphView();
    this.graphView.registerOnNodeClicked(
        node => this.$emit(CUSTOM_EVENTS.NODE_CLICKED, node));
  },
  watch: {
    /**
     * Watching a "ticker" variable is used for now since we don't always want
     * `graphView` to be reactive with respect to `pageModel` (eg. if the user
     * is typing but has not submitted yet). This ticker hence becomes the only
     * way to force the visualization to update its underlying data.
     */
    graphDataUpdateTicker: function() {
      this.graphView.updateGraphData(this.pageModel.getDataForD3());
    },
  },
  template: `
    <div id="graph-container">
      <svg id="graph-svg" width="960" height="600"></svg>
    </div>`,
};

export {
  GraphVisualization,
};
