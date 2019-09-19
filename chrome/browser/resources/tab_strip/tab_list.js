// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {CustomElement} from './custom_element.js';
import {TabElement} from './tab.js';
import {TabsApiProxy} from './tabs_api_proxy.js';

/** @const {number} */
const GHOST_PINNED_TAB_COUNT = 3;

/**
 * The amount of padding to leave between the edge of the screen and the active
 * tab when auto-scrolling. This should leave some room to show the previous or
 * next tab to afford to users that there more tabs if the user scrolls.
 * @const {number}
 */
const SCROLL_PADDING = 32;

/**
 * @param {!Element} element
 * @return {boolean}
 */
function isTabElement(element) {
  return element.tagName === 'TABSTRIP-TAB';
}

class TabListElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /**
     * A chain of promises that the tab list needs to keep track of. The chain
     * is useful in cases when the list needs to wait for all animations to
     * finish in order to get accurate pixels (such as getting the position of a
     * tab) or accurate element counts.
     * @type {!Promise}
     */
    this.animationPromises = Promise.resolve();

    /**
     * The TabElement that is currently being dragged.
     * @private {!TabElement|undefined}
     */
    this.draggedItem_;

    /** @private {!Element} */
    this.pinnedTabsContainerElement_ =
        /** @type {!Element} */ (
            this.shadowRoot.querySelector('#pinnedTabsContainer'));

    /** @private {!Element} */
    this.scrollingParent_ = document.documentElement;

    /** @private {!TabsApiProxy} */
    this.tabsApi_ = TabsApiProxy.getInstance();

    /** @private {!Object} */
    this.tabsApiHandler_ = this.tabsApi_.callbackRouter;

    /** @private {!Element} */
    this.tabsContainerElement_ =
        /** @type {!Element} */ (
            this.shadowRoot.querySelector('#tabsContainer'));

    /** @private {number} */
    this.windowId_;

    addWebUIListener(
        'tab-thumbnail-updated', this.tabThumbnailUpdated_.bind(this));

    this.addEventListener(
        'dragstart', (e) => this.onDragStart_(/** @type {!DragEvent} */ (e)));
    this.addEventListener(
        'dragend', (e) => this.onDragEnd_(/** @type {!DragEvent} */ (e)));
    this.addEventListener(
        'dragover', (e) => this.onDragOver_(/** @type {!DragEvent} */ (e)));
  }

  /**
   * @param {!Promise} promise
   * @private
   */
  addAnimationPromise_(promise) {
    this.animationPromises = this.animationPromises.then(() => promise);
  }

  connectedCallback() {
    this.tabsApi_.getCurrentWindow().then((currentWindow) => {
      this.windowId_ = currentWindow.id;

      // TODO(johntlee): currentWindow.tabs is guaranteed to be defined because
      // `populate: true` is passed in as part of the arguments to the API.
      // Once the closure compiler is able to type `assert` to return a truthy
      // type even when being used with modules, the conditionals should be
      // replaced with `assert` (b/138729777).
      if (currentWindow.tabs) {
        for (const tab of currentWindow.tabs) {
          if (tab) {
            this.onTabCreated_(tab);
          }
        }

        const activeTab = this.getActiveTab_();
        if (activeTab) {
          this.scrollToTab_(activeTab);
        }
      }

      this.tabsApiHandler_.onActivated.addListener(
          this.onTabActivated_.bind(this));
      this.tabsApiHandler_.onCreated.addListener(this.onTabCreated_.bind(this));
      this.tabsApiHandler_.onMoved.addListener(this.onTabMoved_.bind(this));
      this.tabsApiHandler_.onRemoved.addListener(this.onTabRemoved_.bind(this));
      this.tabsApiHandler_.onUpdated.addListener(this.onTabUpdated_.bind(this));
    });
  }

  /**
   * @param {!Tab} tab
   * @return {!TabElement}
   * @private
   */
  createTabElement_(tab) {
    const tabElement = new TabElement();
    tabElement.tab = tab;
    return tabElement;
  }

  /**
   * @param {number} tabId
   * @return {?TabElement}
   * @private
   */
  findTabElement_(tabId) {
    return /** @type {?TabElement} */ (
        this.shadowRoot.querySelector(`tabstrip-tab[data-tab-id="${tabId}"]`));
  }

  /**
   * @return {?TabElement}
   * @private
   */
  getActiveTab_() {
    return /** @type {?TabElement} */ (
        this.shadowRoot.querySelector('tabstrip-tab[active]'));
  }

  /**
   * @return {number}
   * @private
   */
  getPinnedTabsCount_() {
    return this.pinnedTabsContainerElement_.childElementCount -
        GHOST_PINNED_TAB_COUNT;
  }

  /**
   * @param {!TabElement} tabElement
   * @param {number} index
   * @private
   */
  insertTabOrMoveTo_(tabElement, index) {
    // Remove the tabElement if it already exists in the DOM
    tabElement.remove();

    if (tabElement.tab && tabElement.tab.pinned) {
      this.pinnedTabsContainerElement_.insertBefore(
          tabElement, this.pinnedTabsContainerElement_.childNodes[index]);
    } else {
      // Pinned tabs are in their own container, so the index of non-pinned
      // tabs need to be offset by the number of pinned tabs
      const offsetIndex = index - this.getPinnedTabsCount_();
      this.tabsContainerElement_.insertBefore(
          tabElement, this.tabsContainerElement_.childNodes[offsetIndex]);
    }

    this.updatePinnedTabsState_();
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  onDragEnd_(event) {
    if (!this.draggedItem_) {
      return;
    }

    this.draggedItem_.setDragging(false);
    this.draggedItem_ = undefined;
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  onDragOver_(event) {
    event.preventDefault();
    const dragOverItem = event.path.find((pathItem) => {
      return pathItem !== this.draggedItem_ && isTabElement(pathItem);
    });

    if (!dragOverItem ||
        dragOverItem.tab.pinned !== this.draggedItem_.tab.pinned) {
      // TODO(johntlee): Support dragging between different pinned states.
      return;
    }

    let dragOverIndex =
        Array.from(dragOverItem.parentNode.children).indexOf(dragOverItem);
    event.dataTransfer.dropEffect = 'move';
    if (!dragOverItem.tab.pinned) {
      dragOverIndex += this.getPinnedTabsCount_();
    }

    this.tabsApi_.moveTab(this.draggedItem_.tab.id, dragOverIndex);
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  onDragStart_(event) {
    const draggedItem = event.path[0];
    if (!isTabElement(draggedItem)) {
      return;
    }

    this.draggedItem_ = /** @type {!TabElement} */ (draggedItem);
    this.draggedItem_.setDragging(true);
    event.dataTransfer.effectAllowed = 'move';
    event.dataTransfer.setDragImage(
        this.draggedItem_.getDragImage(),
        event.pageX - this.draggedItem_.offsetLeft,
        event.pageY - this.draggedItem_.offsetTop);
  }

  /**
   * @param {!TabActivatedInfo} activeInfo
   * @private
   */
  onTabActivated_(activeInfo) {
    if (activeInfo.windowId !== this.windowId_) {
      return;
    }

    const previouslyActiveTab = this.getActiveTab_();
    if (previouslyActiveTab) {
      previouslyActiveTab.tab = /** @type {!Tab} */ (
          Object.assign({}, previouslyActiveTab.tab, {active: false}));
    }

    const newlyActiveTab = this.findTabElement_(activeInfo.tabId);
    if (newlyActiveTab) {
      newlyActiveTab.tab = /** @type {!Tab} */ (
          Object.assign({}, newlyActiveTab.tab, {active: true}));
      this.scrollToTab_(newlyActiveTab);
    }
  }

  /**
   * @param {!Tab} tab
   * @private
   */
  onTabCreated_(tab) {
    if (tab.windowId !== this.windowId_) {
      return;
    }

    const tabElement = this.createTabElement_(tab);
    this.insertTabOrMoveTo_(tabElement, tab.index);
    this.addAnimationPromise_(tabElement.slideIn());
  }

  /**
   * @param {number} tabId
   * @param {!TabMovedInfo} moveInfo
   * @private
   */
  onTabMoved_(tabId, moveInfo) {
    if (moveInfo.windowId !== this.windowId_) {
      return;
    }

    const movedTab = this.findTabElement_(tabId);
    if (movedTab) {
      this.insertTabOrMoveTo_(movedTab, moveInfo.toIndex);
      if (movedTab.tab.active) {
        this.scrollToTab_(movedTab);
      }
    }
  }

  /**
   * @param {number} tabId
   * @param {!WindowRemoveInfo} removeInfo
   * @private
   */
  onTabRemoved_(tabId, removeInfo) {
    if (removeInfo.windowId !== this.windowId_) {
      return;
    }

    const tabElement = this.findTabElement_(tabId);
    if (tabElement) {
      this.addAnimationPromise_(new Promise(async resolve => {
        await tabElement.slideOut();
        this.updatePinnedTabsState_();
        resolve();
      }));
    }
  }

  /**
   * @param {number} tabId
   * @param {!Tab} changeInfo
   * @param {!Tab} tab
   * @private
   */
  onTabUpdated_(tabId, changeInfo, tab) {
    if (tab.windowId !== this.windowId_) {
      return;
    }

    const tabElement = this.findTabElement_(tabId);
    if (tabElement) {
      tabElement.tab = tab;

      if (changeInfo.pinned !== undefined) {
        // If the tab is being pinned or unpinned, we need to move it to its new
        // location
        this.insertTabOrMoveTo_(tabElement, tab.index);
        if (tab.active) {
          this.scrollToTab_(tabElement);
        }
      }
    }
  }

  /**
   * @param {!TabElement} tabElement
   * @private
   */
  scrollToTab_(tabElement) {
    this.animationPromises.then(() => {
      const screenLeft = this.scrollingParent_.scrollLeft;
      const screenRight = screenLeft + this.scrollingParent_.offsetWidth;

      if (screenLeft > tabElement.offsetLeft) {
        // If the element's left is to the left of the visible screen, scroll
        // such that the element's left edge is aligned with the screen's edge
        this.scrollingParent_.scrollLeft =
            tabElement.offsetLeft - SCROLL_PADDING;
      } else if (screenRight < tabElement.offsetLeft + tabElement.offsetWidth) {
        // If the element's right is to the right of the visible screen, scroll
        // such that the element's right edge is aligned with the screen's right
        // edge.
        this.scrollingParent_.scrollLeft = tabElement.offsetLeft +
            tabElement.offsetWidth - this.scrollingParent_.offsetWidth +
            SCROLL_PADDING;
      }
    });
  }

  /**
   * @param {number} tabId
   * @param {string} imgData
   * @private
   */
  tabThumbnailUpdated_(tabId, imgData) {
    const tab = this.findTabElement_(tabId);
    if (tab) {
      tab.updateThumbnail(imgData);
    }
  }

  /** @private */
  updatePinnedTabsState_() {
    this.pinnedTabsContainerElement_.toggleAttribute(
        'empty',
        this.pinnedTabsContainerElement_.childElementCount ===
            GHOST_PINNED_TAB_COUNT);
  }
}

customElements.define('tabstrip-tab-list', TabListElement);
