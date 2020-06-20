// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Data structures for representing a directed graph.
 */

import {GraphStore} from './graph_store.js';

/** A node in a directed graph. */
class Node {
  /**
   * @param {string} id The unique ID for the node.
   * @param {string} displayName The name to display when visualizing this node.
   */
  constructor(id, displayName) {
    /** @public @const {string} */
    this.id = id;
    /** @public @const {string} */
    this.displayName = displayName;

    // This information (edges) already exists in Graph.edges. However, we
    // duplicate it here to make BFS traversal more efficient.
    /** @public {!Set<!Node>} */
    this.inbound = new Set();
    /** @public {!Set<!Node>} */
    this.outbound = new Set();
  }

  /**
   * Adds a node to the inbound set of this node.
   * @param {!Node} other The inbound node.
   */
  addInbound(other) {
    this.inbound.add(other);
  }

  /**
   * Adds a node to the outbound set of this node.
   * @param {!Node} other The outbound node.
   */
  addOutbound(other) {
    this.outbound.add(other);
  }
}

/**
 * An edge in a directed graph.
 */
class Edge {
  /**
   * @param {string} id The unique ID for the edge.
   * @param {!Node} source The source Node object.
   * @param {!Node} target The target Node object.
   */
  constructor(id, source, target) {
    /** @public @const {string} */
    this.id = id;
    /** @public @const {!Node} */
    this.source = source;
    /** @public @const {!Node} */
    this.target = target;
  }
}

/**
 * Generates and returns a unique edge ID from its source/target Node IDs.
 * @param {string} sourceId The ID of the source node.
 * @param {string} targetId The ID of the target node.
 * @return {string} The ID uniquely identifying the edge source -> target.
 */
function getEdgeIdFromNodes(sourceId, targetId) {
  return `${sourceId} > ${targetId}`;
}

/** A directed graph. */
class Graph {
  constructor() {
    /** @public {!Map<string, !Node>} */
    this.nodes = new Map();
    /** @public {!Map<string, !Edge>} */
    this.edges = new Map();
  }

  /**
   * Adds a Node to the node set.
   * @param {!Node} node The node to add.
   */
  addNodeIfNew(node) {
    if (!this.nodes.has(node.id)) {
      this.nodes.set(node.id, node);
    }
  }

  /**
   * Retrieves a Node from the node set, if it exists.
   * @param {string} id The ID of the desired node.
   * @return {?Node} The Node if it exists, otherwise null.
   */
  getNodeById(id) {
    return this.nodes.get(id) || null;
  }

  /**
   * Creates and adds an Edge to the edge set.
   * Also updates the inbound/outbound sets of the edge's nodes.
   * @param {!Node} sourceNode The node at the start of the edge.
   * @param {!Node} targetNode The node at the end of the edge.
   */
  addEdgeIfNew(sourceNode, targetNode) {
    const edgeId = getEdgeIdFromNodes(sourceNode.id, targetNode.id);
    if (!this.edges.has(edgeId)) {
      const edge = new Edge(edgeId, sourceNode, targetNode);
      this.edges.set(edgeId, edge);
      sourceNode.addOutbound(targetNode);
      targetNode.addInbound(sourceNode);
    }
  }

  /**
   * Retrieves the list of nodes for visualization with d3.
   * @param {GraphStore} filter The filter to apply to the data set.
   * @return {Array<Node>} The nodes to visualize.
   */
  getNodesForD3(filter) {
    const resultNodes = [];
    for (const node of this.nodes.values()) {
      if (filter.includedNodeSet.has(node.id)) {
        resultNodes.push(node);
      }
    }
    return resultNodes;
  }

  /**
   * Retrieves the list of edges for visualization with d3.
   * @param {GraphStore} filter The filter to apply to the data set.
   * @return {Array<Edge>} The edges to visualize.
   */
  getEdgesForD3(filter) {
    const resultEdges = [];
    for (const edge of this.edges.values()) {
      if (filter.includedNodeSet.has(edge.source.id) &&
          filter.includedNodeSet.has(edge.target.id)) {
        resultEdges.push(edge);
      }
    }
    return resultEdges;
  }
}

export {
  Node,
  Edge,
  Graph,
};
