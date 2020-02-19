// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {isTabElement, TabElement} from './tab.js';
import {isTabGroupElement, TabGroupElement} from './tab_group.js';
import {TabsApiProxy} from './tabs_api_proxy.js';

/**
 * Gets the data type of tab IDs on DataTransfer objects in drag events. This
 * is a function so that loadTimeData can get overridden by tests.
 * @return {string}
 */
function getTabIdDataType() {
  return loadTimeData.getString('tabIdDataType');
}

/** @return {string} */
function getGroupIdDataType() {
  return loadTimeData.getString('tabGroupIdDataType');
}

/**
 * @interface
 */
export class DragManagerDelegate {
  /**
   * @param {!TabElement} tabElement
   * @return {number}
   */
  getIndexOfTab(tabElement) {}

  /** @param {!Element} element */
  showDropPlaceholder(element) {}
}

/** @typedef {!DragManagerDelegate|!HTMLElement} */
let DragManagerDelegateElement;

export class DragManager {
  /** @param {!DragManagerDelegateElement} delegate */
  constructor(delegate) {
    /** @private {!DragManagerDelegateElement} */
    this.delegate_ = delegate;

    /**
     * The element currently being dragged.
     * @type {?TabElement|?TabGroupElement}
     */
    this.draggedItem_ = null;

    /** @type {!Element} */
    this.dropPlaceholder_ = document.createElement('div');
    this.dropPlaceholder_.id = 'dropPlaceholder';

    /** @private {!TabsApiProxy} */
    this.tabsProxy_ = TabsApiProxy.getInstance();
  }

  cancelDrag() {
    this.dropPlaceholder_.remove();
  }

  /** @param {!DragEvent} event */
  continueDrag(event) {
    event.preventDefault();

    if (!this.draggedItem_) {
      this.delegate_.showDropPlaceholder(this.dropPlaceholder_);
      return;
    }

    event.dataTransfer.dropEffect = 'move';
    if (isTabGroupElement(this.draggedItem_)) {
      this.continueDragWithGroupElement_(event);
    } else if (isTabElement(this.draggedItem_)) {
      this.continueDragWithTabElement_(event);
    }
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  continueDragWithGroupElement_(event) {
    const composedPath = /** @type {!Array<!Element>} */ (event.composedPath());
    if (composedPath.includes(assert(this.draggedItem_))) {
      // Dragging over itself or a child of itself.
      return;
    }

    const dragOverTabElement =
        /** @type {!TabElement|undefined} */ (composedPath.find(isTabElement));
    if (dragOverTabElement && !dragOverTabElement.tab.pinned) {
      const dragOverIndex = this.delegate_.getIndexOfTab(dragOverTabElement);
      this.tabsProxy_.moveGroup(
          this.draggedItem_.dataset.groupId, dragOverIndex);
      return;
    }

    const dragOverGroupElement = composedPath.find(isTabGroupElement);
    if (dragOverGroupElement) {
      const dragOverIndex = this.delegate_.getIndexOfTab(
          /** @type {!TabElement} */ (dragOverGroupElement.firstElementChild));
      this.tabsProxy_.moveGroup(
          this.draggedItem_.dataset.groupId, dragOverIndex);
    }
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  continueDragWithTabElement_(event) {
    const composedPath = /** @type {!Array<!Element>} */ (event.composedPath());
    const dragOverTabElement =
        /** @type {?TabElement} */ (composedPath.find(isTabElement));
    if (dragOverTabElement &&
        dragOverTabElement.tab.pinned !== this.draggedItem_.tab.pinned) {
      // Can only drag between the same pinned states.
      return;
    }

    const dragOverTabGroup =
        /** @type {?TabGroupElement} */ (composedPath.find(isTabGroupElement));
    if (dragOverTabGroup &&
        dragOverTabGroup.dataset.groupId !== this.draggedItem_.tab.groupId) {
      this.tabsProxy_.groupTab(
          this.draggedItem_.tab.id, dragOverTabGroup.dataset.groupId);
      return;
    }

    if (!dragOverTabGroup && this.draggedItem_.tab.groupId) {
      this.tabsProxy_.ungroupTab(this.draggedItem_.tab.id);
      return;
    }

    if (!dragOverTabElement) {
      return;
    }

    const dragOverIndex = this.delegate_.getIndexOfTab(dragOverTabElement);
    this.tabsProxy_.moveTab(this.draggedItem_.tab.id, dragOverIndex);
  }

  /**
   * @param {!DragEvent} event
   */
  drop(event) {
    if (this.draggedItem_) {
      // If there is a valid dragged item, the drag originated from this TabList
      // and is handled already by previous dragover events.
      return;
    }

    this.dropPlaceholder_.remove();

    if (event.dataTransfer.types.includes(getTabIdDataType())) {
      const tabId = Number(event.dataTransfer.getData(getTabIdDataType()));
      if (Number.isNaN(tabId)) {
        // Invalid tab ID. Return silently.
        return;
      }
      this.tabsProxy_.moveTab(tabId, -1);
    } else if (event.dataTransfer.types.includes(getGroupIdDataType())) {
      const groupId = event.dataTransfer.getData(getGroupIdDataType());
      this.tabsProxy_.moveGroup(groupId, -1);
    }
  }

  /** @param {!DragEvent} event */
  startDrag(event) {
    const draggedItem =
        /** @type {!Array<!Element>} */ (event.composedPath()).find(item => {
          return isTabElement(item) || isTabGroupElement(item);
        });
    if (!draggedItem) {
      return;
    }

    this.draggedItem_ = /** @type {!TabElement} */ (draggedItem);
    event.dataTransfer.effectAllowed = 'move';
    const draggedItemRect = this.draggedItem_.getBoundingClientRect();
    this.draggedItem_.setDragging(true);
    event.dataTransfer.setDragImage(
        this.draggedItem_.getDragImage(), event.clientX - draggedItemRect.left,
        event.clientY - draggedItemRect.top);

    if (isTabElement(draggedItem)) {
      event.dataTransfer.setData(
          getTabIdDataType(), this.draggedItem_.tab.id.toString());
    } else if (isTabGroupElement(draggedItem)) {
      event.dataTransfer.setData(
          getGroupIdDataType(), this.draggedItem_.dataset.groupId);
    }
  }

  /** @param {!DragEvent} event */
  stopDrag(event) {
    if (!this.draggedItem_) {
      return;
    }

    this.draggedItem_.setDragging(false);
    this.draggedItem_ = null;
  }
}
