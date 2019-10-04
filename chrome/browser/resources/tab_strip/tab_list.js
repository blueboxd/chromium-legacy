// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {addWebUIListener} from 'chrome://resources/js/cr.m.js';

import {CustomElement} from './custom_element.js';
import {TabElement} from './tab.js';
import {TabStripViewProxy} from './tab_strip_view_proxy.js';
import {TabsApiProxy} from './tabs_api_proxy.js';

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
     * Attach and detach callbacks require async requests and therefore may
     * cause race conditions in which the async requests complete after another
     * event has been dispatched. Therefore, this object is necessary to keep
     * track of the recent attached or detached state of each tab to ensure
     * elements are not created when they should not be. A truthy value
     * signifies the tab is attached to the current window.
     * @private {!Object<number, boolean>}
     */
    this.attachmentStates_ = {};

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

    /** @private {!TabStripViewProxy} */
    this.tabStripViewProxy_ = TabStripViewProxy.getInstance();

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

    addWebUIListener('theme-changed', () => this.fetchAndUpdateColors_());
    this.tabStripViewProxy_.observeThemeChanges();

    addWebUIListener(
        'tab-thumbnail-updated', this.tabThumbnailUpdated_.bind(this));

    this.addEventListener(
        'dragstart', (e) => this.onDragStart_(/** @type {!DragEvent} */ (e)));
    this.addEventListener(
        'dragend', (e) => this.onDragEnd_(/** @type {!DragEvent} */ (e)));
    this.addEventListener(
        'dragover', (e) => this.onDragOver_(/** @type {!DragEvent} */ (e)));
    document.addEventListener(
        'visibilitychange', () => this.moveOrScrollToActiveTab_());
  }

  /**
   * @param {!Promise} promise
   * @private
   */
  addAnimationPromise_(promise) {
    this.animationPromises = this.animationPromises.then(() => promise);
  }

  connectedCallback() {
    this.fetchAndUpdateColors_();
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

        this.moveOrScrollToActiveTab_();
      }

      this.tabsApiHandler_.onAttached.addListener(
          (tabId, attachedInfo) => this.onTabAttached_(tabId, attachedInfo));
      this.tabsApiHandler_.onActivated.addListener(
          (activeInfo) => this.onTabActivated_(activeInfo));
      this.tabsApiHandler_.onCreated.addListener(
          (tab) => this.onTabCreated_(tab));
      this.tabsApiHandler_.onDetached.addListener(
          (tabId, detachInfo) => this.onTabDetached_(tabId, detachInfo));
      this.tabsApiHandler_.onMoved.addListener(
          (tabId, moveInfo) => this.onTabMoved_(tabId, moveInfo));
      this.tabsApiHandler_.onRemoved.addListener(
          (tabId, removeInfo) => this.onTabRemoved_(tabId, removeInfo));
      this.tabsApiHandler_.onUpdated.addListener(
          (tabId, changeInfo, tab) =>
              this.onTabUpdated_(tabId, changeInfo, tab));
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

  /** @private */
  fetchAndUpdateColors_() {
    this.tabStripViewProxy_.getColors().then(colors => {
      for (const [cssVariable, rgbaValue] of Object.entries(colors)) {
        this.style.setProperty(cssVariable, rgbaValue);
      }
    });
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
      const offsetIndex =
          index - this.pinnedTabsContainerElement_.childElementCount;
      this.tabsContainerElement_.insertBefore(
          tabElement, this.tabsContainerElement_.childNodes[offsetIndex]);
    }
  }

  /** @private */
  moveOrScrollToActiveTab_() {
    const activeTab = this.getActiveTab_();
    if (!activeTab) {
      return;
    }

    if (!this.tabStripViewProxy_.isVisible() && !activeTab.tab.pinned &&
        this.tabsContainerElement_.firstChild !== activeTab) {
      this.tabsApi_.moveTab(
          activeTab.tab.id, this.pinnedTabsContainerElement_.childElementCount);
    } else {
      this.scrollToTab_(activeTab);
    }
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

    if (!dragOverItem || !this.draggedItem_ || !dragOverItem.tab.pinned) {
      return;
    }

    event.dataTransfer.dropEffect = 'move';

    const dragOverIndex =
        Array.from(dragOverItem.parentNode.children).indexOf(dragOverItem);
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

    assert(draggedItem.tab.pinned);
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
      this.moveOrScrollToActiveTab_();
    }
  }

  /**
   * @param {number} tabId
   * @param {!TabAttachedInfo} attachInfo
   * @private
   */
  async onTabAttached_(tabId, attachInfo) {
    if (attachInfo.newWindowId !== this.windowId_) {
      return;
    }

    this.attachmentStates_[tabId] = true;
    const tab = await this.tabsApi_.getTab(tabId);
    if (this.attachmentStates_[tabId] && !this.findTabElement_(tabId)) {
      const tabElement = this.createTabElement_(tab);
      this.insertTabOrMoveTo_(tabElement, attachInfo.newPosition);
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

    if (tab.active && !tab.pinned &&
        tab.index !== this.pinnedTabsContainerElement_.childElementCount) {
      // Newly created active tabs should first be moved to the very beginning
      // of the tab strip to enforce the tab strip's most recently used ordering
      this.tabsApi_
          .moveTab(tab.id, this.pinnedTabsContainerElement_.childElementCount)
          .then(() => {
            this.insertTabOrMoveTo_(
                tabElement, this.pinnedTabsContainerElement_.childElementCount);
            this.addAnimationPromise_(tabElement.slideIn());
          });
    } else {
      this.insertTabOrMoveTo_(tabElement, tab.index);
      this.addAnimationPromise_(tabElement.slideIn());
    }
  }

  /**
   * @param {number} tabId
   * @param {!TabDetachedInfo} detachInfo
   * @private
   */
  onTabDetached_(tabId, detachInfo) {
    if (detachInfo.oldWindowId !== this.windowId_) {
      return;
    }

    this.attachmentStates_[tabId] = false;
    const tabElement = this.findTabElement_(tabId);
    if (tabElement) {
      tabElement.remove();
    }
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
      // While a tab may go in and out of a loading state, the Extensions API
      // only dispatches |onUpdated| events up until the first time a tab
      // reaches a non-loading state. Therefore, the UI should ignore any
      // updates to a |status| of a tab unless the API specifically has
      // dispatched an event indicating the status has changed.
      if (!changeInfo.status) {
        tab.status = tabElement.tab.status;
      }

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
}

customElements.define('tabstrip-tab-list', TabListElement);
