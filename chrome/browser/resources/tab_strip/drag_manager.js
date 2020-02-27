// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {isTabElement, TabElement} from './tab.js';
import {isTabGroupElement, TabGroupElement} from './tab_group.js';
import {TabData, TabNetworkState, TabsApiProxy} from './tabs_api_proxy.js';

/** @const {number} */
export const PLACEHOLDER_TAB_ID = -1;

/**
 * The data type key for pinned state of a tab. Since drag events only expose
 * whether or not a data type exists (not the actual value), presence of this
 * data type means that the tab is pinned.
 * @const {string}
 */
const PINNED_DATA_TYPE = 'pinned';

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

/** @return {!TabData} */
function getDefaultTabData() {
  return {
    active: false,
    alertStates: [],
    blocked: false,
    crashed: false,
    id: -1,
    index: -1,
    isDefaultFavicon: false,
    networkState: TabNetworkState.NONE,
    pinned: false,
    shouldHideThrobber: false,
    showIcon: true,
    title: '',
    url: '',
  };
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

  /**
   * @param {!TabElement} element
   * @param {number} index
   * @param {boolean} pinned
   * @param {string=} groupId
   */
  placeTabElement(element, index, pinned, groupId) {}

  /**
   * @param {!TabGroupElement} element
   * @param {number} index
   */
  placeTabGroupElement(element, index) {}

  /** @param {!Element} element */
  showDropPlaceholder(element) {}
}

/** @typedef {!DragManagerDelegate|!HTMLElement} */
let DragManagerDelegateElement;

class DragSession {
  /**
   * @param {!DragManagerDelegateElement} delegate
   * @param {!TabElement|!TabGroupElement} element
   * @param {number} srcIndex
   * @param {string=} srcGroup
   */
  constructor(delegate, element, srcIndex, srcGroup) {
    /** @const @private {!DragManagerDelegateElement} */
    this.delegate_ = delegate;

    /** @const {!TabElement|!TabGroupElement} */
    this.element_ = element;

    /** @const {number} */
    this.srcIndex = srcIndex;

    /** @const {string|undefined} */
    this.srcGroup = srcGroup;

    /** @private @const {!TabsApiProxy} */
    this.tabsProxy_ = TabsApiProxy.getInstance();
  }

  /**
   * @param {!DragManagerDelegateElement} delegate
   * @param {!TabElement|!TabGroupElement} element
   * @return {!DragSession}
   */
  static createFromElement(delegate, element) {
    if (isTabGroupElement(element)) {
      return new DragSession(
          delegate, element,
          delegate.getIndexOfTab(
              /** @type {!TabElement} */ (element.firstElementChild)));
    }

    const srcIndex = delegate.getIndexOfTab(
        /** @type {!TabElement} */ (element));
    const srcGroup =
        (element.parentElement && isTabGroupElement(element.parentElement)) ?
        element.parentElement.dataset.groupId :
        undefined;
    return new DragSession(delegate, element, srcIndex, srcGroup);
  }

  /**
   * @param {!DragManagerDelegateElement} delegate
   * @param {!DragEvent} event
   * @return {?DragSession}
   */
  static createFromEvent(delegate, event) {
    if (event.dataTransfer.types.includes(getTabIdDataType())) {
      const isPinned = event.dataTransfer.types.includes('pinned');
      const placeholderTabElement =
          /** @type {!TabElement} */ (document.createElement('tabstrip-tab'));
      placeholderTabElement.tab = /** @type {!TabData} */ (Object.assign(
          getDefaultTabData(), {id: PLACEHOLDER_TAB_ID, pinned: isPinned}));
      placeholderTabElement.setDragging(true);
      delegate.placeTabElement(placeholderTabElement, -1, isPinned);
      return DragSession.createFromElement(delegate, placeholderTabElement);
    }

    return null;
  }

  /** @return {string|undefined} */
  get dstGroup() {
    if (isTabElement(this.element_) && this.element_.parentElement &&
        isTabGroupElement(this.element_.parentElement)) {
      return this.element_.parentElement.dataset.groupId;
    }

    return undefined;
  }

  /** @return {number} */
  get dstIndex() {
    if (isTabElement(this.element_)) {
      return this.delegate_.getIndexOfTab(
          /** @type {!TabElement} */ (this.element_));
    }

    // If a tab group is moving backwards (to the front of the tab strip), the
    // new index is the index of the first tab in that group. If a tab group is
    // moving forwards (to the end of the tab strip), the new index is the index
    // of the last tab in that group.
    let dstIndex = this.delegate_.getIndexOfTab(
        /** @type {!TabElement} */ (this.element_.firstElementChild));
    if (this.srcIndex <= dstIndex) {
      dstIndex += this.element_.childElementCount - 1;
    }
    return dstIndex;
  }

  cancel() {
    if (this.isDraggingPlaceholder()) {
      this.element_.remove();
      return;
    }

    if (isTabGroupElement(this.element_)) {
      this.delegate_.placeTabGroupElement(
          /** @type {!TabGroupElement} */ (this.element_), this.srcIndex);
    } else if (isTabElement(this.element_)) {
      this.delegate_.placeTabElement(
          /** @type {!TabElement} */ (this.element_), this.srcIndex,
          this.element_.tab.pinned, this.srcGroup);
    }

    this.element_.setDragging(false);
  }

  /** @return {boolean} */
  isDraggingPlaceholder() {
    return isTabElement(this.element_) &&
        this.element_.tab.id == PLACEHOLDER_TAB_ID;
  }

  /** @param {!DragEvent} event */
  finish(event) {
    if (this.isDraggingPlaceholder()) {
      const id = Number(event.dataTransfer.getData(getTabIdDataType()));
      this.element_.tab = Object.assign({}, this.element_.tab, {id});
    }

    const dstIndex = this.dstIndex;
    if (isTabElement(this.element_)) {
      this.tabsProxy_.moveTab(this.element_.tab.id, dstIndex);
    } else if (isTabGroupElement(this.element_)) {
      this.tabsProxy_.moveGroup(this.element_.dataset.groupId, dstIndex);
    }

    const dstGroup = this.dstGroup;
    if (dstGroup && dstGroup !== this.srcGroup) {
      this.tabsProxy_.groupTab(this.element_.tab.id, dstGroup);
    } else if (!dstGroup && this.srcGroup) {
      this.tabsProxy_.ungroupTab(this.element_.tab.id);
    }

    this.element_.setDragging(false);
  }

  /** @param {!DragEvent} event */
  start(event) {
    event.dataTransfer.effectAllowed = 'move';
    const draggedItemRect = this.element_.getBoundingClientRect();
    this.element_.setDragging(true);
    event.dataTransfer.setDragImage(
        this.element_.getDragImage(), event.clientX - draggedItemRect.left,
        event.clientY - draggedItemRect.top);

    if (isTabElement(this.element_)) {
      event.dataTransfer.setData(
          getTabIdDataType(), this.element_.tab.id.toString());

      if (this.element_.tab.pinned) {
        event.dataTransfer.setData(
            'pinned', this.element_.tab.pinned.toString());
      }
    } else if (isTabGroupElement(this.element_)) {
      event.dataTransfer.setData(
          getGroupIdDataType(), this.element_.dataset.groupId);
    }
  }

  /** @param {!DragEvent} event */
  update(event) {
    event.dataTransfer.dropEffect = 'move';
    if (isTabGroupElement(this.element_)) {
      this.updateForTabGroupElement_(event);
    } else if (isTabElement(this.element_)) {
      this.updateForTabElement_(event);
    }
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  updateForTabGroupElement_(event) {
    const tabGroupElement =
        /** @type {!TabGroupElement} */ (this.element_);
    const composedPath = /** @type {!Array<!Element>} */ (event.composedPath());
    if (composedPath.includes(assert(this.element_))) {
      // Dragging over itself or a child of itself.
      return;
    }

    const dragOverTabElement =
        /** @type {!TabElement|undefined} */ (composedPath.find(isTabElement));
    if (dragOverTabElement && !dragOverTabElement.tab.pinned) {
      const dragOverIndex = this.delegate_.getIndexOfTab(dragOverTabElement);
      this.delegate_.placeTabGroupElement(tabGroupElement, dragOverIndex);
      return;
    }

    const dragOverGroupElement = composedPath.find(isTabGroupElement);
    if (dragOverGroupElement) {
      const dragOverIndex = this.delegate_.getIndexOfTab(
          /** @type {!TabElement} */ (dragOverGroupElement.firstElementChild));
      this.delegate_.placeTabGroupElement(tabGroupElement, dragOverIndex);
    }
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  updateForTabElement_(event) {
    const tabElement = /** @type {!TabElement} */ (this.element_);
    const composedPath = /** @type {!Array<!Element>} */ (event.composedPath());
    const dragOverTabElement =
        /** @type {?TabElement} */ (composedPath.find(isTabElement));
    if (dragOverTabElement &&
        dragOverTabElement.tab.pinned !== tabElement.tab.pinned) {
      // Can only drag between the same pinned states.
      return;
    }

    const previousGroupId = (tabElement.parentElement &&
                             isTabGroupElement(tabElement.parentElement)) ?
        tabElement.parentElement.dataset.groupId :
        undefined;

    const dragOverTabGroup =
        /** @type {?TabGroupElement} */ (composedPath.find(isTabGroupElement));
    if (dragOverTabGroup &&
        dragOverTabGroup.dataset.groupId !== previousGroupId) {
      this.delegate_.placeTabElement(
          tabElement, this.dstIndex, false, dragOverTabGroup.dataset.groupId);
      return;
    }

    if (!dragOverTabGroup && previousGroupId) {
      this.delegate_.placeTabElement(
          tabElement, this.dstIndex, false, undefined);
      return;
    }

    if (!dragOverTabElement) {
      return;
    }

    const dragOverIndex = this.delegate_.getIndexOfTab(dragOverTabElement);
    this.delegate_.placeTabElement(
        tabElement, dragOverIndex, tabElement.tab.pinned, previousGroupId);
  }
}

export class DragManager {
  /** @param {!DragManagerDelegateElement} delegate */
  constructor(delegate) {
    /** @private {!DragManagerDelegateElement} */
    this.delegate_ = delegate;

    /** @type {?DragSession} */
    this.dragSession_ = null;

    /** @type {!Element} */
    this.dropPlaceholder_ = document.createElement('div');
    this.dropPlaceholder_.id = 'dropPlaceholder';

    /** @private {!TabsApiProxy} */
    this.tabsProxy_ = TabsApiProxy.getInstance();
  }

  /** @private */
  onDragLeave_() {
    if (this.dragSession_ && this.dragSession_.isDraggingPlaceholder()) {
      this.dragSession_.cancel();
      this.dragSession_ = null;
    }

    // TODO(johntlee): Handle drag and drop groups from other windows with
    // DragSession.
    this.dropPlaceholder_.remove();
  }

  /** @param {!DragEvent} event */
  onDragOver_(event) {
    event.preventDefault();

    if (!this.dragSession_) {
      // TODO(johntlee): Handle drag and drop groups from other windows with
      // DragSession.
      this.delegate_.showDropPlaceholder(this.dropPlaceholder_);
      return;
    }

    this.dragSession_.update(event);
  }

  /** @param {!DragEvent} event */
  onDragStart_(event) {
    const draggedItem =
        /** @type {!Array<!Element>} */ (event.composedPath()).find(item => {
          return isTabElement(item) || isTabGroupElement(item);
        });
    if (!draggedItem) {
      return;
    }

    this.dragSession_ = DragSession.createFromElement(
        this.delegate_,
        /** @type {!TabElement|!TabGroupElement} */ (draggedItem));
    this.dragSession_.start(event);
  }

  /** @param {!DragEvent} event */
  onDragEnd_(event) {
    if (!this.dragSession_) {
      return;
    }

    this.dragSession_.cancel();
    this.dragSession_ = null;
  }

  /** @param {!DragEvent} event */
  onDragEnter_(event) {
    if (this.dragSession_) {
      return;
    }

    this.dragSession_ = DragSession.createFromEvent(this.delegate_, event);
  }

  /**
   * @param {!DragEvent} event
   */
  onDrop_(event) {
    if (this.dragSession_) {
      this.dragSession_.finish(event);
      this.dragSession_ = null;
      return;
    }

    // TODO(johntlee): Handle drag and drop groups from other windows with
    // DragSession.
    this.dropPlaceholder_.remove();
    if (event.dataTransfer.types.includes(getGroupIdDataType())) {
      const groupId = event.dataTransfer.getData(getGroupIdDataType());
      this.tabsProxy_.moveGroup(groupId, -1);
    }
  }

  startObserving() {
    this.delegate_.addEventListener(
        'dragstart', e => this.onDragStart_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener(
        'dragend', e => this.onDragEnd_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener(
        'dragenter', (e) => this.onDragEnter_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener('dragleave', () => this.onDragLeave_());
    this.delegate_.addEventListener(
        'dragover', e => this.onDragOver_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener(
        'drop', e => this.onDrop_(/** @type {!DragEvent} */ (e)));
  }
}
