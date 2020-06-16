// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Data structures for representing a directed graph.
 */

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
 * Note that source/target are stored as strings, not Node objects.
 */
class Edge {
  /**
   * @param {string} source The ID of the source node.
   * @param {string} target The ID of the target node.
   */
  constructor(source, target) {
    /** @public @const {string} */
    this.id = `${source} > ${target}`;
    /** @public @const {string} */
    this.source = source;
    /** @public @const {string} */
    this.target = target;
  }
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
   * Adds an Edge to the edge set.
   * Also updates the inbound/outbound sets of the edge's nodes, if they exist.
   * @param {!Edge} edge The edge to add.
   */
  addEdgeIfNew(edge) {
    if (!this.edges.has(edge.id)) {
      this.edges.set(edge.id, edge);
      const sourceNode = this.getNodeById(edge.source);
      if (sourceNode !== null) {
        const targetNode = this.getNodeById(edge.target);
        if (targetNode !== null) {
          sourceNode.addOutbound(targetNode);
          targetNode.addInbound(sourceNode);
        }
      }
    }
  }

  /**
   * Retrieves the list of nodes for visualization with d3.
   * @return {Array<Node>} The nodes to visualize.
   */
  getNodesForD3() {
    return [...this.nodes.values()];
  }

  /**
   * Retrieves the list of edges for visualization with d3.
   * @return {Array<Edge>} The edges to visualize.
   */
  getEdgesForD3() {
    return [...this.edges.values()];
  }
}

export {
  Node,
  Edge,
  Graph,
};
