// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GraphNode, D3GraphData} from './graph_model.js';
import {DisplaySettingsData, GraphEdgeColor} from './page_model.js';

import * as d3 from 'd3';

// The perpendicular distance from the center of an edge to place the control
// point for a quadratic Bezier curve.
const EDGE_CURVE_OFFSET = {
  CURVED: 30,
  STRAIGHT: 0,
};

// The default color for edges.
const DEFAULT_EDGE_COLOR = '#999';

// A map from GraphEdgeColor to its start and end color codes. The property
// `targetDefId` will be used as the unique ID of the colored arrow in the SVG
// defs (https://developer.mozilla.org/en-US/docs/Web/SVG/Element/defs), and the
// arrowhead will be referred to by `url(#targetDefId)` in the SVG.
const EDGE_COLORS = {
  [GraphEdgeColor.DEFAULT]: {
    source: DEFAULT_EDGE_COLOR,
    target: DEFAULT_EDGE_COLOR,
    targetDefId: 'graph-arrowhead-default',
  },
  [GraphEdgeColor.GREY_GRADIENT]: {
    source: '#ddd',
    target: '#666',
    targetDefId: 'graph-arrowhead-grey-gradient',
  },
  [GraphEdgeColor.BLUE_TO_RED]: {
    source: '#00f',
    target: '#f00',
    targetDefId: 'graph-arrowhead-blue-to-red',
  },
};

// Parameters determining the force-directed simulation cooldown speed.
// For more info on each param, see https://github.com/d3/d3-force.
const SIMULATION_SPEED_PARAMS = {
  // Alpha (or temperature, as in simulated annealing) is reset to this value
  // for the simulation is reheated.
  ALPHA_ON_REHEAT: 0.3,

  // Alpha at which simulation freezes.
  ALPHA_MIN: 0.001,

  // A higher velocity decay indicates slower nodes (node movement is multiplied
  // by (1-decay) each tick).

  // The decay to use when there is no easing (eg. dragging nodes).
  VELOCITY_DECAY_DEFAULT: 0.8,

  // When nodes are eased, their decay starts at MAX (slow),
  // transitions to MIN (fast), then back to MAX.
  VELOCITY_DECAY_MAX: 0.99,
  VELOCITY_DECAY_MIN: 0,
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
 * @param {string} id The HTML id for the arrowhead.
 * @param {string} color The color of the arrowhead.
 * @param {number} length The length of the arrowhead.
 * @param {number} width The width of the arrowhead.
 */
function addArrowMarkerDef(defs, id, color, length, width) {
  const halfWidth = Math.floor(width / 2);
  defs.append('marker')
      .attr('id', id) // 'graph-arrowhead-*'
      .attr('viewBox', `0 -${halfWidth} ${length} ${width}`)
      // TODO(yjlong): 5 is the hardcoded radius, change for dynamic radius.
      .attr('refX', length + 5)
      .attr('refY', 0)
      .attr('orient', 'auto')
      .attr('markerWidth', length)
      .attr('markerHeight', width)
      .append('path')
      .attr('d', `M 0 -${halfWidth} L ${length} 0 L 0 ${halfWidth}`)
      .attr('fill', color)
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
 * Wrapper class around the logic for the currently hovered node.
 *
 * The hovered node should not change when being dragged, even if the "real"
 * hovered node (as determined by mouse position) changes during the drag (e.g.,
 * if the mouse moves too fast). Update takes place when drag ends, i.e., the
 * dragged node become unhovered after drag if the mouse came off the node.
 */
class HoveredNodeManager {
  constructor() {
    /** @public {?GraphNode} */
    this.hoveredNode = null;
    /** @private {?GraphNode} */
    this.realHoveredNode_ = null;
    /** @private {boolean} */
    this.isDragging_ = false;
  }
  setDragging(isDragging) {
    this.isDragging_ = isDragging;
    if (!this.isDragging_) {
      // When the drag ends, update the hovered node.
      this.hoveredNode = this.realHoveredNode_;
    }
  }
  setHoveredNode(hoveredNode) {
    this.realHoveredNode_ = hoveredNode;
    if (!this.isDragging_) {
      // The hovered node can only be updated when not dragging.
      this.hoveredNode = this.realHoveredNode_;
    }
  }
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
    /** @private {number} */
    this.edgeCurveOffset_ = EDGE_CURVE_OFFSET.CURVED;
    /** @private {boolean} */
    this.colorEdgesOnlyOnHover_ = true;
    /** @private {string} */
    this.graphEdgeColor_ = GraphEdgeColor.DEFAULT;
    /** @private {!HoveredNodeManager} */
    this.hoveredNodeManager_ = new HoveredNodeManager();

    /** @private @type {?OnNodeClickedCallback} */
    this.onNodeClicked_ = null;

    const svg = d3.select('#graph-svg');
    const graphGroup = svg.append('g'); // Contains entire graph (for zoom/pan).
    this.svgDefs_ = svg.append('defs');

    // Add an arrowhead def for every possible edge target color.
    for (const {target, targetDefId} of Object.values(EDGE_COLORS)) {
      addArrowMarkerDef(this.svgDefs_, targetDefId, target, 10, 6);
    }

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
        .attr('stroke-width', 1)
        .attr('fill', 'transparent');
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

    const centeringStrengthY = 0.1;
    const centeringStrengthX = centeringStrengthY * (height / width);
    /** @private {*} */
    this.simulation_ = d3.forceSimulation()
        .alphaMin(SIMULATION_SPEED_PARAMS.ALPHA_MIN)
        .force('chargeForce', d3.forceManyBody().strength(-3000))
        .force('centerXForce',
            d3.forceX(width / 2).strength(node => {
              if (node.visualizationState.selectedByFilter) {
                return centeringStrengthX * 20;
              }
              if (node.visualizationState.outboundDepth <= 1) {
                return centeringStrengthX * 5;
              }
              return centeringStrengthY;
            }))
        .force('centerYForce',
            d3.forceY(height / 2).strength(node => {
              if (node.visualizationState.selectedByFilter) {
                return centeringStrengthY * 20;
              }
              if (node.visualizationState.outboundDepth <= 1) {
                return centeringStrengthY * 5;
              }
              return centeringStrengthY;
            }));

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
        .range([SIMULATION_SPEED_PARAMS.VELOCITY_DECAY_MIN,
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

  /** Synchronizes the path of all edges to their underlying data. */
  syncEdgePaths() {
    this.edgeGroup_.selectAll('path')
        .attr('d', edge => {
          // To calculate the control point, consider the edge vector [dX, dY]:
          // * Flip over Y axis (since SVGs increase y downwards): [dX, -dY]
          // * Rotate 90 degrees clockwise: [-dY, -dX]
          // * Normalize and multiply by curve offset: offset/norm * [-dY, -dX]
          // * Flip over Y again to get SVG coords: offset/norm * [-dY, dX]
          // * Add to midpoint: [midX, midY] + offset/norm * [-dY, dX]
          const deltaX = edge.target.x - edge.source.x;
          const deltaY = edge.target.y - edge.source.y;
          if (deltaX === 0 && deltaY === 0) {
            return null; // Do not draw paths for self-edges.
          }

          const midX = (edge.source.x + edge.target.x) / 2;
          const midY = (edge.source.y + edge.target.y) / 2;

          const scaleFactor =
            this.edgeCurveOffset_ / Math.hypot(deltaX, deltaY);
          const controlPointX = midX - scaleFactor * deltaY;
          const controlPointY = midY + scaleFactor * deltaX;

          const path = d3.path();
          path.moveTo(edge.source.x, edge.source.y);
          path.quadraticCurveTo(
              controlPointX, controlPointY, edge.target.x, edge.target.y);
          return path.toString();
        });
  }

  /** Synchronizes the color of all edges to their underlying data. */
  syncEdgeColors() {
    const hoveredNode = this.hoveredNodeManager_.hoveredNode;
    const edgeTouchesHoveredNode = edge =>
      edge.source === hoveredNode || edge.target === hoveredNode;
    const nodeTouchesHoveredNode = node => {
      return node === hoveredNode ||
        hoveredNode.inbound.has(node) || hoveredNode.outbound.has(node);
    };

    // Point the associated gradient in the direction of the line.
    this.svgDefs_.selectAll('linearGradient')
        .attr('x1', edge => edge.source.x)
        .attr('y1', edge => edge.source.y)
        .attr('x2', edge => edge.target.x)
        .attr('y2', edge => edge.target.y);

    this.edgeGroup_.selectAll('path')
        .attr('marker-end', edge => {
          if (edge.source === edge.target) {
            return null;
          } else if (!this.colorEdgesOnlyOnHover_ ||
              edgeTouchesHoveredNode(edge)) {
            return `url(#${EDGE_COLORS[this.graphEdgeColor_].targetDefId})`;
          }
          return `url(#${EDGE_COLORS[GraphEdgeColor.DEFAULT].targetDefId})`;
        })
        .attr('stroke', edge => {
          if (!this.colorEdgesOnlyOnHover_ || edgeTouchesHoveredNode(edge)) {
            return `url(#${edge.id})`;
          }
          return DEFAULT_EDGE_COLOR;
        })
        .classed('non-hovered-edge', edge => {
          return this.colorEdgesOnlyOnHover_ &&
            hoveredNode !== null && !edgeTouchesHoveredNode(edge);
        });

    this.labelGroup_.selectAll('text')
        .classed('non-hovered-text', node => {
          return this.colorEdgesOnlyOnHover_ &&
            hoveredNode !== null && !nodeTouchesHoveredNode(node);
        });
  }

  /** Updates the colors of the edge gradients to match the selected color. */
  syncEdgeGradients() {
    const {source, target} = EDGE_COLORS[this.graphEdgeColor_];
    const gradientSelection = this.svgDefs_.selectAll('linearGradient');
    gradientSelection.selectAll('stop').remove();
    gradientSelection.append('stop')
        .attr('offset', '0%')
        .attr('stop-color', source);
    gradientSelection.append('stop')
        .attr('offset', '100%')
        .attr('stop-color', target);
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
      this.syncEdgePaths();
      this.syncEdgeColors();

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
   * Updates the display settings for the visualization.
   * @param {!DisplaySettingsData} displaySettings The display config.
   */
  updateDisplaySettings(displaySettings) {
    const {
      curveEdges,
      colorOnlyOnHover,
      graphEdgeColor,
    } = displaySettings;
    this.edgeCurveOffset_ = curveEdges ?
      EDGE_CURVE_OFFSET.CURVED : EDGE_CURVE_OFFSET.STRAIGHT;
    this.colorEdgesOnlyOnHover_ = colorOnlyOnHover;
    this.graphEdgeColor_ = graphEdgeColor;

    this.syncEdgeGradients();
    this.syncEdgePaths();
    this.syncEdgeColors();
  }

  /**
   * Updates the data source used for the visualization.
   *
   * @param {!D3GraphData} inputData The new data to use.
   */
  updateGraphData(inputData) {
    const {nodes: inputNodes, edges: inputEdges} = inputData;

    let nodesAddedOrRemoved = false;
    this.svgDefs_.selectAll('linearGradient')
        .data(inputEdges, edge => edge.id)
        .join(enter => enter.append('linearGradient')
            .attr('id', edge => edge.id)
            .attr('gradientUnits', 'userSpaceOnUse'));

    // TODO(yjlong): Determine if we ever want to render self-loops (will need
    // to be a loop instead of a straight line) and handle accordingly.
    this.edgeGroup_.selectAll('path')
        .data(inputEdges, edge => edge.id)
        .join(enter => enter.append('path'));

    this.nodeGroup_.selectAll('circle')
        .data(inputNodes, node => node.id)
        .join(enter => {
          if (!enter.empty()) {
            nodesAddedOrRemoved = true;
          }
          return enter.append('circle')
              .attr('r', 5)
              .on('mousedown', node => this.onNodeClicked_(node))
              .on('mouseenter', node => {
                this.hoveredNodeManager_.setHoveredNode(node);
                this.syncEdgeColors();
              })
              .on('mouseleave', () => {
                this.hoveredNodeManager_.setHoveredNode(null);
                this.syncEdgeColors();
              })
              .call(d3.drag()
                  .on('start', () => this.hoveredNodeManager_.setDragging(true))
                  .on('drag', (node, idx, nodes) => {
                    this.reheatSimulation(/* shouldEase */ false);
                    d3.select(nodes[idx]).classed('locked', true);
                    // Fix the node's position after it has been dragged.
                    node.fx = d3.event.x;
                    node.fy = d3.event.y;
                  })
                  .on('end', () => this.hoveredNodeManager_.setDragging(false)))
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
    this.simulation_
        .nodes(inputNodes)
        .force('links', d3.forceLink(inputEdges).id(edge => edge.id));
    if (nodesAddedOrRemoved) {
      this.simulation_.stop();
      this.reheatSimulation(/* shouldEase */ true);
    }
  }
}

export {
  GraphView,
};
