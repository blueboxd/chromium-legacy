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

// Keys for identifying URL params.
const URL_PARAM_KEYS = {
  FILTER: 'filter',
};

/**
 * Converts a URL to the node filter contained within its querystring.
 * @param {string} url The URL to convert.
 * @return {!Array<string>} The array of node names in the URL's filter, or an
 *     empty array if there was no filter in the URL.
 */
function generateFilterFromUrl(url) {
  const pageUrl = new URL(url);
  const filterNodes = pageUrl.searchParams.get(URL_PARAM_KEYS.FILTER);
  if (filterNodes !== null) {
    return filterNodes.split(',');
  }
  return [];
}

/**
 * Converts a node filter into a URL containing the filter information. The
 * filter information will be stored in the querystring of the supplied URL.
 * @param {!Array<string>} filter The node name filter to store in the URL.
 * @param {string} currentUrl The URL of the current page.
 * @return {string} The new URL containing the filter information.
 */
function generateUrlFromFilter(filter, currentUrl) {
  const filterNameString = filter.join(',');

  const pageUrl = new URL(currentUrl);
  const searchParams = new URLSearchParams();
  if (filter.length > 0) {
    searchParams.append(URL_PARAM_KEYS.FILTER, filterNameString);
  }
  return `${pageUrl.origin}${pageUrl.pathname}?${searchParams.toString()}`;
}

// TODO(yjlong): Currently we take JSON served by a Python server running on
// the side. Replace this with a user upload or pull from some other source.
document.addEventListener('DOMContentLoaded', () => {
  d3.json(`${LOCALHOST}/json_graph.txt`).then(data => {
    const graphModel = parseGraphModelFromJson(data.package_graph);
    const pageModel = new PageModel(graphModel);
    const graphView = new GraphView();
    const pageController = new PageController(pageModel, graphView);

    const includedNodesInUrl = generateFilterFromUrl(document.URL);

    pageController.setOutboundDepth(1);
    if (includedNodesInUrl.length !== 0) {
      pageController.addIncludedNodes(includedNodesInUrl);
    } else {
      // TODO(yjlong): This is test data. Remove this when no longer needed.
      pageController.addIncludedNodes([
        'org.chromium.base',
        'org.chromium.chrome.browser.gsa',
        'org.chromium.chrome.browser.omaha',
        'org.chromium.chrome.browser.media',
        'org.chromium.ui.base',
      ]);
    }


    new Vue({
      el: '#selected-node-details',
      data: pageModel.selectedNodeDetailsData,
      methods: {
        addSelectedToFilter: function() {
          pageController.addIncludedNodes([this.selectedNode.id]);
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
          pageController.addIncludedNodes([this.filterInputText]);
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

    new Vue({
      el: '#url-generator',
      data: pageModel.nodeFilterData,
      methods: {
        /**
         * Generates an URL for the current page containing the filter in its
         * querystring, then copies the URL to the input elem and highlights it.
         */
        generateUrl: function() {
          const pageUrl = generateUrlFromFilter(this.nodeList, document.URL);
          this.$refs.input.value = pageUrl;
          this.$refs.input.select();
        },
      },
    });
  });
});
