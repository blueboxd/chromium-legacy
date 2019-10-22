// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './tab.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {CustomElement} from './custom_element.js';
import {TabElement} from './tab.js';
import {TabStripEmbedderProxy} from './tab_strip_embedder_proxy.js';
import {tabStripOptions} from './tab_strip_options.js';
import {TabData, TabsApiProxy} from './tabs_api_proxy.js';

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

    /** @private {!TabStripEmbedderProxy} */
    this.tabStripEmbedderProxy_ = TabStripEmbedderProxy.getInstance();

    /** @private {!TabsApiProxy} */
    this.tabsApi_ = TabsApiProxy.getInstance();

    /** @private {!Element} */
    this.tabsContainerElement_ =
        /** @type {!Element} */ (
            this.shadowRoot.querySelector('#tabsContainer'));

    addWebUIListener('theme-changed', () => this.fetchAndUpdateColors_());
    this.tabStripEmbedderProxy_.observeThemeChanges();

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

    if (loadTimeData.getBoolean('showDemoOptions')) {
      this.shadowRoot.querySelector('#demoOptions').style.display = 'block';

      const mruCheckbox = this.shadowRoot.querySelector('#mruCheckbox');
      mruCheckbox.checked = tabStripOptions.mruEnabled;
      mruCheckbox.addEventListener('change', () => {
        tabStripOptions.mruEnabled = mruCheckbox.checked;
      });

      const autoCloseCheckbox =
          this.shadowRoot.querySelector('#autoCloseCheckbox');
      autoCloseCheckbox.checked = tabStripOptions.autoCloseEnabled;
      autoCloseCheckbox.addEventListener('change', () => {
        tabStripOptions.autoCloseEnabled = autoCloseCheckbox.checked;
      });
    }
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
    this.tabsApi_.getTabs().then(tabs => {
      tabs.forEach(tab => this.onTabCreated_(tab));
      this.moveOrScrollToActiveTab_();

      addWebUIListener('tab-created', tab => this.onTabCreated_(tab));
      addWebUIListener(
          'tab-moved', (tabId, newIndex) => this.onTabMoved_(tabId, newIndex));
      addWebUIListener('tab-removed', tabId => this.onTabRemoved_(tabId));
      addWebUIListener('tab-updated', tab => this.onTabUpdated_(tab));
      addWebUIListener(
          'tab-active-changed', tabId => this.onTabActivated_(tabId));
    });
  }

  /**
   * @param {!TabData} tab
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
    this.tabStripEmbedderProxy_.getColors().then(colors => {
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

    if (tabStripOptions.mruEnabled &&
        !this.tabStripEmbedderProxy_.isVisible() && !activeTab.tab.pinned &&
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

    if (!dragOverItem || !this.draggedItem_ ||
        dragOverItem.tab.pinned !== this.draggedItem_.tab.pinned) {
      return;
    }

    event.dataTransfer.dropEffect = 'move';

    let dragOverIndex =
        Array.from(dragOverItem.parentNode.children).indexOf(dragOverItem);
    if (!this.draggedItem_.tab.pinned) {
      dragOverIndex += this.pinnedTabsContainerElement_.childElementCount;
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

    if (tabStripOptions.mruEnabled && !draggedItem.tab.pinned) {
      // If MRU is enabled, unpinned tabs should not be draggable.
      event.preventDefault();
      return;
    }

    if (tabStripOptions.mruEnabled) {
      assert(draggedItem.tab.pinned);
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
   * @param {number} tabId
   * @private
   */
  onTabActivated_(tabId) {
    // There may be more than 1 TabElement marked as active if other events
    // have updated a Tab to have an active state. For example, if a
    // tab is created with an already active state, there may be 2 active
    // TabElements: the newly created tab and the previously active tab.
    this.shadowRoot.querySelectorAll('tabstrip-tab[active]')
        .forEach((previouslyActiveTab) => {
          if (previouslyActiveTab.tab.id !== tabId) {
            previouslyActiveTab.tab = /** @type {!TabData} */ (
                Object.assign({}, previouslyActiveTab.tab, {active: false}));
          }
        });

    const newlyActiveTab = this.findTabElement_(tabId);
    if (newlyActiveTab) {
      newlyActiveTab.tab = /** @type {!TabData} */ (
          Object.assign({}, newlyActiveTab.tab, {active: true}));
      this.moveOrScrollToActiveTab_();
    }
  }

  /**
   * @param {!TabData} tab
   * @private
   */
  onTabCreated_(tab) {
    const tabElement = this.createTabElement_(tab);

    if (tabStripOptions.mruEnabled && tab.active && !tab.pinned &&
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
   * @param {number} newIndex
   * @private
   */
  onTabMoved_(tabId, newIndex) {
    const movedTab = this.findTabElement_(tabId);
    if (movedTab) {
      this.insertTabOrMoveTo_(movedTab, newIndex);
      if (movedTab.tab.active) {
        this.scrollToTab_(movedTab);
      }
    }
  }

  /**
   * @param {number} tabId
   * @private
   */
  onTabRemoved_(tabId) {
    const tabElement = this.findTabElement_(tabId);
    if (tabElement) {
      this.addAnimationPromise_(tabElement.slideOut());
    }
  }

  /**
   * @param {!TabData} tab
   * @private
   */
  onTabUpdated_(tab) {
    const tabElement = this.findTabElement_(tab.id);
    if (!tabElement) {
      return;
    }

    const previousTab = tabElement.tab;
    tabElement.tab = tab;

    if (previousTab.pinned !== tab.pinned) {
      // If the tab is being pinned or unpinned, we need to move it to its new
      // location
      this.insertTabOrMoveTo_(tabElement, tab.index);
      if (tab.active) {
        this.scrollToTab_(tabElement);
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
