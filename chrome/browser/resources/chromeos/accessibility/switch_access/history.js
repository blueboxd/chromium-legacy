// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** This class is a structure to store previous state for restoration. */
class FocusData {
  /**
   * @param {!SARootNode} group
   * @param {!SAChildNode} focus Must be a child of |group|.
   */
  constructor(group, focus) {
    /** @public {!SARootNode} */
    this.group = group;
    /** @public {!SAChildNode} */
    this.focus = focus;
  }

  /** @return {boolean} */
  isValid() {
    return this.group.isValidGroup();
  }
}

/** This class handles saving and retrieving FocusData. */
class FocusHistory {
  constructor() {
    /** @private {!Array<!FocusData>} */
    this.dataStack_ = [];
  }

  /**
   * Creates the restore data to get from the desktop node to the specified
   * automation node.
   * Erases the current history and replaces with the new data.
   * @param {!chrome.automation.AutomationNode} node
   */
  buildFromAutomationNode(node) {
    // Create a list of ancestors.
    const ancestorStack = [node];
    while (node.parent) {
      ancestorStack.push(node.parent);
      node = node.parent;
    }

    this.dataStack_ = [];
    let group = DesktopNode.build(NavigationManager.desktopNode);
    while (ancestorStack.length > 0) {
      const candidate = ancestorStack.pop();
      if (candidate.role === chrome.automation.RoleType.DESKTOP ||
          !SwitchAccessPredicate.isInteresting(candidate, group)) {
        continue;
      }

      const focus = group.findChild(candidate);
      if (!focus) {
        continue;
      }
      this.dataStack_.push(new FocusData(group, focus));

      group = focus.asRootNode();
      if (!group) {
        break;
      }
    }
  }

  /**
   * @param {!function(!FocusData): boolean} predicate
   * @return {boolean}
   */
  containsDataMatchingPredicate(predicate) {
    for (const data of this.dataStack_) {
      if (predicate(data)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns the most proximal restore data, but does not remove it from the
   * history.
   * @return {?FocusData}
   */
  peek() {
    return this.dataStack_[this.dataStack_.length - 1] || null;
  }

  /** @return {!FocusData} */
  retrieve() {
    let data = this.dataStack_.pop();
    while (data && !data.isValid()) {
      data = this.dataStack_.pop();
    }

    if (data) {
      return data;
    }

    // If we don't have any valid history entries, fallback to the desktop node.
    const desktop = new DesktopNode(NavigationManager.desktopNode);
    return new FocusData(desktop, desktop.firstChild);
  }

  /** @param {!FocusData} data */
  save(data) {
    this.dataStack_.push(data);
  }

  /** Support for this type being used in for..of loops. */
  [Symbol.iterator]() {
    return this.dataStack_[Symbol.iterator]();
  }
}
