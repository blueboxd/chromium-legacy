// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab.js';

import {CustomElement} from './custom_element.js';
import {TabElement} from './tab.js';
import {TabsApiProxy} from './tabs_api_proxy.js';

const GHOST_PINNED_TAB_COUNT = 3;

class TabListElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!Element} */
    this.pinnedTabsContainerElement_ =
        /** @type {!Element} */ (
            this.shadowRoot.querySelector('#pinnedTabsContainer'));

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
      }

      this.tabsApiHandler_.onActivated.addListener(
          this.onTabActivated_.bind(this));
      this.tabsApiHandler_.onCreated.addListener(this.onTabCreated_.bind(this));
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
      const offsetIndex = index -
          (this.pinnedTabsContainerElement_.childElementCount -
           GHOST_PINNED_TAB_COUNT);
      this.tabsContainerElement_.insertBefore(
          tabElement, this.tabsContainerElement_.childNodes[offsetIndex]);
    }

    this.updatePinnedTabsState_();
  }

  /**
   * @param {!TabActivatedInfo} activeInfo
   * @private
   */
  onTabActivated_(activeInfo) {
    if (activeInfo.windowId !== this.windowId_) {
      return;
    }

    const previouslyActiveTab =
        this.shadowRoot.querySelector('tabstrip-tab[active]');
    if (previouslyActiveTab) {
      previouslyActiveTab.tab = /** @type {!Tab} */ (
          Object.assign({}, previouslyActiveTab.tab, {active: false}));
    }

    const newlyActiveTab = this.findTabElement_(activeInfo.tabId);
    newlyActiveTab.tab = /** @type {!Tab} */ (
        Object.assign({}, newlyActiveTab.tab, {active: true}));
  }

  /**
   * @param {!Tab} tab
   * @private
   */
  onTabCreated_(tab) {
    if (tab.windowId !== this.windowId_) {
      return;
    }

    this.insertTabOrMoveTo_(this.createTabElement_(tab), tab.index);
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
      tabElement.remove();
      this.updatePinnedTabsState_();
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
      }
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
