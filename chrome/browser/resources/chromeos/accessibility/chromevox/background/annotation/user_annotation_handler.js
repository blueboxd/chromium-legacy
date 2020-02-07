// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages ChromeVox users' custom annotations.
 */

goog.provide('UserAnnotationHandler');

goog.require('NodeIdentifier');

/**
 * Stores annotation-related data.
 * @typedef {{
 *    annotation: !string,
 *    identifier: !NodeIdentifier
 * }}
 */
let AnnotationData;

UserAnnotationHandler = class {
  static init() {
    UserAnnotationHandler.instance = new UserAnnotationHandler();
  }

  /**
   * @private
   */
  constructor() {
    /**
     * Stores user annotations.
     * Maps URLs to arrays of AnnotationData objects for that page.
     * @private {Object<string, !Array<!AnnotationData>>}
     */
    this.annotations_ = {};
    /**
     * Whether this feature is enabled or not.
     * @type {boolean}
     * @private
     */
    this.enabled_ = false;
    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-chromevox-annotations',
        (enabled) => {
          this.enabled_ = enabled;
        });
  }

  /**
   * Creates a new, or updates an existing, annotation for the node.
   * @param {!AutomationNode} node
   * @param {!string} annotation
   */
  static setAnnotationForNode(node, annotation) {
    const url = node.root.docUrl || '';
    if (!UserAnnotationHandler.instance.enabled_ || !url) {
      return;
    }

    const annotationData = {annotation, identifier: new NodeIdentifier(node)};

    if (!UserAnnotationHandler.instance.annotations_[url]) {
      UserAnnotationHandler.instance.annotations_[url] = [];
    }

    // Either update an existing annotation or add a new one.
    const annotations = UserAnnotationHandler.instance.annotations_[url];
    const target = annotationData.identifier;
    let updated = false;
    for (let i = 0; i < annotations.length; ++i) {
      if (target.equals(annotations[i].identifier)) {
        UserAnnotationHandler.instance.annotations_[url][i] = annotationData;
        updated = true;
        break;
      }
    }
    if (!updated) {
      UserAnnotationHandler.instance.annotations_[url].push(annotationData);
    }
  }

  /**
   * Returns the annotation for node, if one exists. Otherwise, returns null.
   * @param {!AutomationNode} node
   * @return {?string}
   */
  static getAnnotationForNode(node) {
    const url = node.root.docUrl || '';
    if (!UserAnnotationHandler.instance.enabled_ || !url) {
      return null;
    }

    const candidates = UserAnnotationHandler.instance.annotations_[url];
    if (!candidates) {
      return null;
    }

    const target = new NodeIdentifier(node);
    for (let i = 0; i < candidates.length; ++i) {
      const candidate = candidates[i];
      if (target.equals(candidate.identifier)) {
        return candidate.annotation;
      }
    }

    return null;
  }
};