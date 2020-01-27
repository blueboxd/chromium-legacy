// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a class used to identify AutomationNodes.
 */

goog.provide('NodeIdentifier');

/**
 * Stores all identifying attributes for an AutomationNode.
 * A helper object for NodeIdentifier.
 * @typedef {{
 *    id: !string,
 *    name: !string,
 *    role: !string,
 *    description: !string,
 *    restriction: !string,
 *    childCount: number,
 *    indexInParent: number,
 *    className: !string,
 *    htmlTag: !string }}
 */
var Attributes;

NodeIdentifier = class {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    /**
     * @type {!Attributes}
     */
    this.attributes = this.createAttributes_(node);

    /**
     * @type {!string}
     */
    this.pageUrl = node.root.docUrl || '';

    /**
     * @type {!Array<!Attributes>}
     */
    this.ancestry = this.createAttributesAncestry_(node);
  }

  /**
   * Returns true if |this| is equal to |other|.
   * @param {!NodeIdentifier} other
   * @return {boolean}
   */
  equals(other) {
    // If pageUrl and HTML Id match, we know they refer to the same node.
    if (this.pageUrl && this.attributes.id && this.pageUrl === other.pageUrl &&
        this.attributes.id === other.attributes.id) {
      return true;
    }

    // Ensure both NodeIdentifiers are composed of matching Attributes.
    if (!this.matchingAttributes_(this.attributes, other.attributes)) {
      return false;
    }

    if (this.ancestry.length !== other.ancestry.length) {
      return false;
    }

    for (var i = 0; i < this.ancestry.length; ++i) {
      if (!this.matchingAttributes_(this.ancestry[i], other.ancestry[i])) {
        return false;
      }
    }
    return true;
  }

  /**
   * @param {!AutomationNode} node
   * @return {!Attributes}
   * @private
   */
  createAttributes_(node) {
    return {
      id: (node.htmlAttributes) ? node.htmlAttributes['id'] || '' : '',
      name: node.name || '',
      role: node.role || '',
      description: node.description || '',
      restriction: node.restriction || '',
      childCount: node.childCount || 0,
      indexInParent: node.indexInParent || 0,
      className: node.className || '',
      htmlTag: node.htmlTag || ''
    };
  }

  /**
   * @param {!AutomationNode} node
   * @return {!Array<!Attributes>}
   * @private
   */
  createAttributesAncestry_(node) {
    var ancestry = [];
    var scanNode = node.parent;
    var treeRoot = node.root;
    while (scanNode && scanNode !== treeRoot) {
      ancestry.push(this.createAttributes_(scanNode));
      scanNode = scanNode.parent;
    }
    return ancestry;
  }

  /**
   * @param {!Attributes} target
   * @param {!Attributes} candidate
   * @return {boolean}
   * @private
   */
  matchingAttributes_(target, candidate) {
    for (var [key, targetValue] of Object.entries(target)) {
      if (candidate[key] !== targetValue) {
        return false;
      }
    }
    return true;
  }
};
