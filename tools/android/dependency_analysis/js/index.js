// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {parseGraphModelFromJson} from './process_graph_json.js';
import {PageModel} from './page_model.js';
import {PageController} from './page_controller.js';
import {GraphView} from './graph_view.js';

// For ease of development, we currently serve all our JSON and other assets
// through a simple Python server at localhost:8888. This should be changed
// as we find other ways to serve the assets (user upload or hosted externally).
const LOCALHOST = 'http://localhost:8888';

// TODO(yjlong): Currently we take JSON served by a Python server running on
// the side. Replace this with a user upload or pull from some other source.
document.addEventListener('DOMContentLoaded', () => {
  d3.json(`${LOCALHOST}/json_graph.txt`).then(data => {
    const graphModel = parseGraphModelFromJson(data.package_graph);
    const pageModel = new PageModel(graphModel);
    const graphView = new GraphView();
    const pageController = new PageController(pageModel, graphView);

    // TODO(yjlong): This is test data. Remove this when no longer needed.
    pageController.addIncludedNode('org.chromium.base');
    pageController.addIncludedNode('org.chromium.chrome.browser.gsa');
    pageController.addIncludedNode('org.chromium.chrome.browser.omaha');
    pageController.addIncludedNode('org.chromium.chrome.browser.media');
    pageController.addIncludedNode('org.chromium.ui.base');
    pageController.setOutboundDepth(1);

    new Vue({
      el: '#selected-node-details',
      data: pageModel.selectedNodeDetailsData,
      methods: {
        addSelectedToFilter: function() {
          pageController.addIncludedNode(this.selectedNode.id);
        },
        removeSelectedFromFilter: function() {
          pageController.removeIncludedNode(this.selectedNode.id);
        },
      },
    });

    new Vue({
      el: '#filter-input-group',
      data: {
        filterInputText: '',
      },
      methods: {
        submitFilter: function() {
          pageController.addIncludedNode(this.filterInputText);
        },
      },
    });

    new Vue({
      el: '#filter-items',
      data: pageModel.nodeFilterData,
      methods: {
        removeFilter: function(e) {
          const filterText = e.target.textContent;
          pageController.removeIncludedNode(filterText.trim());
        },
      },
    });

    new Vue({
      el: '#filter-inbound-group',
      data: pageModel.inboundDepthData,
      methods: {
        submitInbound: function() {
          pageController.setInboundDepth(this.inboundDepth);
        },
      },
    });

    new Vue({
      el: '#filter-outbound-group',
      data: pageModel.outboundDepthData,
      methods: {
        submitOutbound: function() {
          pageController.setOutboundDepth(this.outboundDepth);
        },
      },
    });
  });
});
