# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility classes (and functions, in the future) for graph operations."""


class Node(object):  # pylint: disable=useless-object-inheritance
    """A node/vertex in a directed graph.

    Attributes:
        name: A unique string representation of the node.
        inbound: A set of Nodes that have a directed edge into this Node.
        outbound: A set of Nodes that this Node has a directed edge into.
    """
    def __init__(self, unique_key: str):
        """Initializes a new node with the given key.

        Args:
            unique_key: A key uniquely identifying the node.
        """
        self._unique_key = unique_key
        self._outbound = set()
        self._inbound = set()

    def __eq__(self, other: "Node"):  # pylint: disable=missing-function-docstring
        return self._unique_key == other._unique_key

    def __hash__(self):  # pylint: disable=missing-function-docstring
        return hash(self._unique_key)

    @property
    def name(self):
        """A unique string representation of the node."""
        return self._unique_key

    @property
    def inbound(self):
        """A set of Nodes that have a directed edge into this Node."""
        return self._inbound

    @property
    def outbound(self):
        """A set of Nodes that this Node has a directed edge into."""
        return self._outbound

    def add_outbound(self, node: "Node"):
        """Creates an edge from the current node to the provided node."""
        self._outbound.add(node)

    def add_inbound(self, node: "Node"):
        """Creates an edge from the provided node to the current node."""
        self._inbound.add(node)


class Graph(object):  # pylint: disable=useless-object-inheritance
    """A directed graph data structure.

    Maintains an internal Dict[str, Node] _key_to_node
    mapping the unique key of nodes to their Node objects.

    Attributes:
        num_nodes: The number of nodes in the graph.
        num_edges: The number of edges in the graph.
    """
    def __init__(self):  # pylint: disable=missing-function-docstring
        self._key_to_node = {}

    @property
    def num_nodes(self):  # pylint: disable=missing-function-docstring
        return len(self._key_to_node)

    @property
    def num_edges(self):  # pylint: disable=missing-function-docstring
        return sum(len(node.outbound) for node in self._key_to_node.values())

    def get_node_by_key(self, key: str):  # pylint: disable=missing-function-docstring
        return self._key_to_node.get(key)

    def create_node_from_key(self, key: str):
        """Given a unique key, creates and returns a Node object.

        Should be overridden by child classes.
        """
        return Node(key)

    def add_node_if_new(self, key: str):
        """Adds a Node to the graph.

        A new Node object is constructed from the given key and added.
        If the key already exists in the graph, this is a no-op.

        Args:
            key: A unique key to create the new Node from.
        """
        if key not in self._key_to_node:
            self._key_to_node[key] = self.create_node_from_key(key)

    def add_edge_if_new(self, src: str, dest: str):
        """Adds a directed edge to the graph.

        The source and destination nodes are created and added if they
        do not already exist. If the edge already exists in the graph,
        this is a no-op.

        Args:
            src: A unique key representing the source node.
            dest: A unique key representing the destination node.
        """
        self.add_node_if_new(src)
        self.add_node_if_new(dest)
        src_node = self.get_node_by_key(src)
        dest_node = self.get_node_by_key(dest)
        src_node.add_outbound(dest_node)
        dest_node.add_inbound(src_node)
