// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Node, D3GraphData} from './graph_model.js';
import {parseGraphModelFromJson} from './process_graph_json.js';
import {GraphStore} from './graph_store.js';
import {eventBus, eventBusEvents} from './event_bus.js';

// For ease of development, we currently serve all our JSON and other assets
// through a simple Python server at localhost:8888. This should be changed
// as we find other ways to serve the assets (user upload or hosted externally).
const LOCALHOST = 'http://localhost:8888';

// Colors for displayed nodes. These colors are temporary.
const NODE_COLORS = {
  FILTER: d3.color('red'),
  INBOUND: d3.color('#000080'), // Dark blue.
  OUTBOUND: d3.color('#666600'), // Dark yellow.
  INBOUND_AND_OUTBOUND: d3.color('#006400'), // Dark green.
};

/**
 * Computes the color to display for a given node.
 * @param {!Node} node The node in question.
 * @return {string} The color of the node.
 */
function getNodeColor(node) {
  if (node.visualizationState.selectedByFilter) {
    return NODE_COLORS.FILTER;
  }
  if (node.visualizationState.selectedByInbound &&
    node.visualizationState.selectedByOutbound) {
    return NODE_COLORS.INBOUND_AND_OUTBOUND.brighter(
        Math.min(node.visualizationState.inboundDepth,
            node.visualizationState.outboundDepth));
  }
  if (node.visualizationState.selectedByInbound) {
    return NODE_COLORS.INBOUND.brighter(
        node.visualizationState.inboundDepth);
  }
  if (node.visualizationState.selectedByOutbound) {
    return NODE_COLORS.OUTBOUND.brighter(
        node.visualizationState.outboundDepth);
  }
}

/**
 * Adds a def for an arrowhead (triangle) marker to the SVG.
 * @param {*} defs The d3 selector of the SVG defs.
 * @param {string} id The ID to use for the arrowhead marker.
 * @param {number} length The length of the arrowhead.
 * @param {number} width The width of the arrowhead.
 */
function addArrowMarkerDef(defs, id, length, width) {
  const halfWidth = Math.floor(width / 2);
  defs.append('marker')
      .attr('id', id)
      .attr('viewBox', `0 -${halfWidth} ${length} ${width}`)
      // TODO(yjlong): 5 is the hardcoded radius, change for dynamic radius.
      .attr('refX', length + 5)
      .attr('refY', 0)
      .attr('orient', 'auto')
      .attr('markerWidth', length)
      .attr('markerHeight', width)
      .append('path')
      .attr('d', `M 0 -${halfWidth} L ${length} 0 L 0 ${halfWidth}`)
      .attr('fill', '#999')
      .style('stroke', 'none');
}

/**
 * Uses d3 to render a graph on the screen.
 * Currently just binds to the only 'svg' object, as things get more complex
 * we can maybe change this to bind to a given DOM element if necessary.
 *
 * @param {!D3GraphData} inputData The data to render.
 */
function renderGraph(inputData) {
  // TODO(yjlong): Reorganize this function so that we can selectively rerender
  // (as opposed to removing all elements and rerendering on each call),
  const {nodes: inputNodes, edges: inputEdges} = inputData;
  const svg = d3.select('svg');
  svg.selectAll('*').remove();
  const svgDefs = svg.append('defs');
  const graphGroup = svg.append('g'); // Contains entire graph (for zoom/pan).

  addArrowMarkerDef(svgDefs, 'graph-arrowhead', 10, 4);

  // Set up zoom and pan on entire graph.
  svg.call(d3.zoom()
      .scaleExtent([0.25, 10])
      .on('zoom', () =>
        graphGroup.attr('transform', d3.event.transform),
      ));

  // TODO(yjlong): SVG should be resizable & these values updated.
  const width = +svg.attr('width');
  const height = +svg.attr('height');

  const simulation = d3.forceSimulation()
      .nodes(inputNodes)
      .alphaMin(0.1) // Stop the simulation faster than default (0.001).
      .force('chargeForce', d3.forceManyBody().strength(-300))
      .force('centerForce', d3.forceCenter(width / 2, height / 2))
      .force('links', d3.forceLink(inputEdges).id(d => d.id));
  const reheatSimulation = () => simulation.alpha(0.3).restart();

  // TODO(yjlong): Determine if we ever want to render self-loops (will need to
  // be a loop instead of a straight line) and handle accordingly.
  const edgeGroup = graphGroup.append('g')
      .classed('graph-edges', true)
      .attr('stroke-width', 1);
  const edges = edgeGroup.selectAll('line')
      .data(inputEdges)
      .join(enter => enter.append('line'))
      .attr('marker-end', d =>
        (d.source === d.target) ? null : 'url(#graph-arrowhead)');

  const nodeGroup = graphGroup.append('g')
      .classed('graph-nodes', true)
      .attr('fill', 'red');
  const nodes = nodeGroup.selectAll('circle')
      .data(inputNodes)
      .join(enter => enter.append('circle'))
      .attr('r', 5)
      .attr('fill', d => getNodeColor(d))
      .on('mousedown', d => eventBus.$emit(eventBusEvents.D3_NODE_CLICKED, d))
      .call(d3.drag()
          .on('drag', (d, i, nodes) => {
            reheatSimulation();
            d3.select(nodes[i]).classed('locked', true);
            // Fix the node's position after it has been dragged.
            d.fx = d3.event.x;
            d.fy = d3.event.y;
          }))
      .on('click', (d, i, nodes) => {
        if (d3.event.defaultPrevented) {
          return; // Skip drag events.
        }
        const node = d3.select(nodes[i]);
        if (node.classed('locked')) {
          d.fx = null;
          d.fy = null;
          reheatSimulation();
        } else {
          d.fx = d.x;
          d.fy = d.y;
        }
        node.classed('locked', !node.classed('locked'));
      });

  const labelGroup = graphGroup.append('g')
      .classed('graph-labels', true)
      .attr('pointer-events', 'none');
  const labels = labelGroup.selectAll('text')
      .data(inputNodes)
      .join(enter => enter.append('text'))
      .attr('dx', 12)
      .attr('dy', '.35em')
      .text(d => d.displayName);

  // The simulation updates position variables in the JSON, it's up to us
  // to update the visualization to match on each tick.
  const tickActions = () => {
    nodes
        .attr('cx', d => d.x)
        .attr('cy', d => d.y);

    edges
        .attr('x1', d => d.source.x)
        .attr('y1', d => d.source.y)
        .attr('x2', d => d.target.x)
        .attr('y2', d => d.target.y);

    labels
        .attr('x', d => d.x)
        .attr('y', d => d.y);
  };

  simulation.on('tick', tickActions);
}

// TODO(yjlong): Currently we take JSON served by a Python server running on
// the side. Replace this with a user upload or pull from some other source.
document.addEventListener('DOMContentLoaded', () => {
  d3.json(`${LOCALHOST}/json_graph.txt`).then(data => {
    const graphStore = GraphStore.instance;
    const graphModel = parseGraphModelFromJson(data.package_graph);

    // TODO(yjlong): This is test data. Remove this when no longer needed.
    graphStore.addIncludedNode('org.chromium.base');
    graphStore.addIncludedNode('org.chromium.chrome.browser.gsa');
    graphStore.addIncludedNode('org.chromium.chrome.browser.omaha');
    graphStore.addIncludedNode('org.chromium.chrome.browser.media');
    graphStore.addIncludedNode('org.chromium.ui.base');

    new Vue({
      el: '#selected-node-details',
      data: {
        node: null,
      },
      methods: {
        updateDataWithNode: function(node) {
          this.node = node;
        },
        addSelectedToFilter: function() {
          graphStore.addIncludedNode(this.node.id);
          renderGraph(graphModel.getDataForD3(graphStore));
        },
        removeSelectedFromFilter: function() {
          graphStore.removeIncludedNode(this.node.id);
          renderGraph(graphModel.getDataForD3(graphStore));
        },
      },
      mounted: function() {
        eventBus.$on(eventBusEvents.D3_NODE_CLICKED, this.updateDataWithNode);
      },
      beforeDestroy: function() {
        eventBus.$off(eventBusEvents.D3_NODE_CLICKED, this.updateDataWithNode);
      },
    });

    new Vue({
      el: '#filter-input-group',
      data: {
        filterInputText: '',
        sharedState: graphStore.state,
      },
      methods: {
        submitFilter: function() {
          graphStore.addIncludedNode(this.filterInputText);
          renderGraph(graphModel.getDataForD3(graphStore));
        },
      },
    });

    new Vue({
      el: '#filter-items',
      data: {
        sharedState: graphStore.state,
      },
      methods: {
        removeFilter: function(e) {
          const filterText = e.target.textContent;
          graphStore.removeIncludedNode(filterText.trim());
          renderGraph(graphModel.getDataForD3(graphStore));
        },
      },
    });

    new Vue({
      el: '#filter-inbound-group',
      data: {
        inboundNum: 0,
        sharedState: graphStore.state,
      },
      methods: {
        submitInbound: function() {
          graphStore.setInboundDepth(this.inboundNum);
          renderGraph(graphModel.getDataForD3(graphStore));
        },
      },
    });

    new Vue({
      el: '#filter-outbound-group',
      data: {
        outboundNum: 0,
        sharedState: graphStore.state,
      },
      methods: {
        submitOutbound: function() {
          graphStore.setOutboundDepth(this.outboundNum);
          renderGraph(graphModel.getDataForD3(graphStore));
        },
      },
    });

    renderGraph(graphModel.getDataForD3(graphStore));
  });
});
