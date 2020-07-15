// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GraphNode, D3GraphData} from './graph_model.js';

import * as d3 from 'd3';

// The unique HTML IDs for the SVG's `defs`
// (https://developer.mozilla.org/en-US/docs/Web/SVG/Element/defs)
const DEF_IDS = {
  GRAPH_ARROWHEAD: 'graph-arrowhead',
};

// Parameters determining the force-directed simulation cooldown speed.
// For more info on each param, see https://github.com/d3/d3-force.
const SIMULATION_SPEED_PARAMS = {
  ALPHA_ON_REHEAT: 1,
  ALPHA_MIN: 0.03,
  VELOCITY_DECAY_DEFAULT: 0.4,
  VELOCITY_DECAY_MAX: 0.99,
};

// Colors for displayed nodes. These colors are temporary.
const NODE_COLORS = {
  FILTER: d3.color('red'),
  INBOUND: d3.color('#000080'), // Dark blue.
  OUTBOUND: d3.color('#666600'), // Dark yellow.
  INBOUND_AND_OUTBOUND: d3.color('#006400'), // Dark green.
};

/**
 * Computes the color to display for a given node.
 * @param {!GraphNode} node The node in question.
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
 * @param {*} defs The d3 selection of the SVG defs.
 * @param {number} length The length of the arrowhead.
 * @param {number} width The width of the arrowhead.
 */
function addArrowMarkerDef(defs, length, width) {
  const halfWidth = Math.floor(width / 2);
  defs.append('marker')
      .attr('id', DEF_IDS.GRAPH_ARROWHEAD)
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
 * When we reheat the simulation, we'd like to know how many ticks it will take
 * to cool. Instead of finding an formula for our specific config, we just make
 * a throwaway simulation with our config and run it to completion.
 *
 * @return {number} The number of ticks it will take for a reheat to cool.
 */
function countNumReheatTicks() {
  let reheatTicks = 0;
  const reheatTicksCounter = d3.forceSimulation()
      .alphaMin(SIMULATION_SPEED_PARAMS.ALPHA_MIN)
      .alpha(SIMULATION_SPEED_PARAMS.ALPHA_ON_REHEAT);
  while (reheatTicksCounter.alpha() > reheatTicksCounter.alphaMin()) {
    reheatTicks++;
    reheatTicksCounter.tick();
  }
  return reheatTicks;
}

/**
 * A callback to be triggered whenever a node is clicked in the visualization.
 * @callback OnNodeClickedCallback
 * @param {!GraphNode} node The node that was clicked.
 */

/** The view of the visualization, controlling display on the SVG. */
class GraphView {
  /**
   * Initializes some variables and performs one-time setup of the SVG canvas.
   * Currently just binds to the only 'svg' object, as things get more complex
   * we can maybe change this to bind to a given DOM element if necessary.
   */
  constructor() {
    /** @private @type {?OnNodeClickedCallback} */
    this.onNodeClicked_ = null;

    const svg = d3.select('#graph-svg');
    const graphGroup = svg.append('g'); // Contains entire graph (for zoom/pan).
    const svgDefs = svg.append('defs');

    addArrowMarkerDef(svgDefs, 10, 4);

    // Set up zoom and pan on the entire graph.
    svg.call(d3.zoom()
        .scaleExtent([0.25, 10])
        .on('zoom', () =>
          graphGroup.attr('transform', d3.event.transform),
        ));

    // The order of these groups decide the SVG paint order (since we append
    // sequentially), we want edges below nodes below labels.
    /** @private {*} */
    this.edgeGroup_ = graphGroup.append('g')
        .classed('graph-edges', true)
        .attr('stroke-width', 1);
    /** @private {*} */
    this.nodeGroup_ = graphGroup.append('g')
        .classed('graph-nodes', true)
        .attr('fill', 'red');
    /** @private {*} */
    this.labelGroup_ = graphGroup.append('g')
        .classed('graph-labels', true)
        .attr('pointer-events', 'none');

    // TODO(yjlong): SVG should be resizable & these values updated.
    const width = +svg.attr('width');
    const height = +svg.attr('height');

    const centeringStrengthY = 1.0;
    const centeringStrengthX = centeringStrengthY * (height / width);
    /** @private {*} */
    this.simulation_ = d3.forceSimulation()
        .alphaMin(SIMULATION_SPEED_PARAMS.ALPHA_MIN)
        .force('chargeForce', d3.forceManyBody().strength(-3000))
        .force('centerXForce',
            d3.forceX(width / 2).strength(centeringStrengthX))
        .force('centerYForce',
            d3.forceY(height / 2).strength(centeringStrengthY));

    /** @private {number} */
    this.reheatTicks_ = countNumReheatTicks();

    /**
     * @callback LinearScaler
     * @param {number} input The input value between [0, 1] inclusive.
     * @returns {number} The input scaled linearly to new bounds.
     */
    /** @private {LinearScaler} */
    this.velocityDecayScale_ = d3.scaleLinear()
        .domain([0, 1])
        .range([SIMULATION_SPEED_PARAMS.VELOCITY_DECAY_DEFAULT,
          SIMULATION_SPEED_PARAMS.VELOCITY_DECAY_MAX]);
  }

  /**
   * Binds the event when a node is clicked in the graph to a given callback.
   * @param {!OnNodeClickedCallback} onNodeClicked The callback to bind to.
   */
  registerOnNodeClicked(onNodeClicked) {
    this.onNodeClicked_ = onNodeClicked;
  }

  /**
   * Computes the velocityDecay for our current progress in the reheat process.
   *
   * See https://github.com/d3/d3-force#simulation_velocityDecay. We animate new
   * nodes on the page by starting off with a high decay (slower nodes), easing
   * to a low decay (faster nodes), then easing back to a high decay at the end.
   * This makes the node animation seem more smooth and natural.
   * @param {number} currentTick The number of ticks passed in the reheat.
   * @return {number} The velocityDecay for the current point in the reheat.
   */
  getEasedVelocityDecay(currentTick) {
    // The input to the ease function has to be in [0, 1] inclusive. Since
    // velocity is multiplied by (1 - decay) and we want speeds of
    // slow-fast-slow, the midpoint of the process should correspond to 0 and
    // the beginning and end to 1 (ie. the decay should look like \/).
    const normalizedCurrentTick = Math.abs(
        1 - 2 * (currentTick / this.reheatTicks_));
    const normalizedEaseTick = d3.easeQuadInOut(normalizedCurrentTick);
    // Since the ease returns a value in [0, 1] inclusive, we have to scale it
    // back to our desired velocityDecay range before returning.
    return (this.velocityDecayScale_(normalizedEaseTick));
  }

  /**
   * Reheats the simulation, allowing all nodes to move according to the physics
   * simulation until they cool down again.
   * @param {boolean} shouldEase Whether the node movement should be eased. This
   *     should not be used when dragging nodes, since the speed at the start of
   *     the ease will be used all throughout the drag.
   */
  reheatSimulation(shouldEase) {
    let tickNum = 0;

    // The simulation updates position variables in the data every tick, it's up
    // to us to update the visualization to match.
    const tickActions = () => {
      this.edgeGroup_.selectAll('line')
          .attr('x1', edge => edge.source.x)
          .attr('y1', edge => edge.source.y)
          .attr('x2', edge => edge.target.x)
          .attr('y2', edge => edge.target.y);

      this.nodeGroup_.selectAll('circle')
          .attr('cx', node => node.x)
          .attr('cy', node => node.y);

      this.labelGroup_.selectAll('text')
          .attr('x', label => label.x)
          .attr('y', label => label.y);

      tickNum ++;
      if (shouldEase) {
        this.simulation_.velocityDecay(this.getEasedVelocityDecay(tickNum));
      }
    };

    // If we don't ease, the default decay is sufficient for the entire reheat.
    const startingVelocityDecay = shouldEase ?
      this.getEasedVelocityDecay(0) :
      SIMULATION_SPEED_PARAMS.VELOCITY_DECAY_DEFAULT;

    this.simulation_
        .on('tick', tickActions)
        .velocityDecay(startingVelocityDecay)
        .alpha(SIMULATION_SPEED_PARAMS.ALPHA_ON_REHEAT)
        .restart();
  }

  /**
   * Updates the data source used for the visualization.
   *
   * @param {!D3GraphData} inputData The new data to use.
   */
  updateGraphData(inputData) {
    const {nodes: inputNodes, edges: inputEdges} = inputData;

    this.simulation_
        .nodes(inputNodes)
        .force('links', d3.forceLink(inputEdges).id(edge => edge.id));
    this.simulation_.stop();

    let nodesAddedOrRemoved = false;
    // TODO(yjlong): Determine if we ever want to render self-loops (will need
    // to be a loop instead of a straight line) and handle accordingly.
    this.edgeGroup_.selectAll('line')
        .data(inputEdges, edge => edge.id)
        .join(enter => enter.append('line')
            .attr('marker-end', edge => {
              if ( edge.source === edge.target ) {
                return null;
              }
              return `url(#${DEF_IDS.GRAPH_ARROWHEAD})`;
            }));

    this.nodeGroup_.selectAll('circle')
        .data(inputNodes, node => node.id)
        .join(enter => {
          if (!enter.empty()) {
            nodesAddedOrRemoved = true;
          }
          return enter.append('circle')
              .attr('r', 5)
              .on('mousedown', node => this.onNodeClicked_(node))
              .call(d3.drag()
                  .on('drag', (node, idx, nodes) => {
                    // TODO(yjlong): This should ideally be a call with `false`
                    // (dragging should continue the simulation with constant
                    // velocity), but for some reason the graph goes crazy with
                    // that setting. Investigate and fix.
                    this.reheatSimulation(/* shouldEase */ true);
                    d3.select(nodes[idx]).classed('locked', true);
                    // Fix the node's position after it has been dragged.
                    node.fx = d3.event.x;
                    node.fy = d3.event.y;
                  }))
              .on('click', (node, idx, nodes) => {
                if (d3.event.defaultPrevented) {
                  return; // Skip drag events.
                }
                const pageNode = d3.select(nodes[idx]);
                if (pageNode.classed('locked')) {
                  node.fx = null;
                  node.fy = null;
                  this.reheatSimulation(/* shouldEase */ true);
                } else {
                  node.fx = node.x;
                  node.fy = node.y;
                }
                // TODO(yjlong): Change this so the style is tied to whether the
                // fx/fy are non-null instead of toggling it each time.
                pageNode.classed('locked', !pageNode.classed('locked'));
              });
        },
        update => update,
        exit => {
          if (!exit.empty()) {
            nodesAddedOrRemoved = true;
          }
          // When a node is removed from the SVG, it should lose all
          // position-related data.
          return exit.each(node => {
            node.x = null;
            node.y = null;
            node.fx = null;
            node.fy = null;
          }).remove();
        })
        .attr('fill', node => getNodeColor(node));

    this.labelGroup_.selectAll('text')
        .data(inputNodes, node => node.id)
        .join(enter => enter.append('text')
            .attr('dx', 12)
            .attr('dy', '.35em')
            .text(label => label.displayName));

    // The graph should not be reheated on a no-op (eg. adding a visible node to
    // the filter which doesn't add/remove any new nodes).
    if (nodesAddedOrRemoved) {
      this.reheatSimulation(/* shouldEase */ true);
    }
  }
}

export {
  GraphView,
};
