// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab.js';

import {CustomElement} from './custom_element.js';
import {TabElement} from './tab.js';
import {TabsApiProxy} from './tabs_api_proxy.js';

class TabListElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

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

      const fragment = document.createDocumentFragment();

      // TODO(johntlee): currentWindow.tabs is guaranteed to be defined because
      // `populate: true` is passed in as part of the arguments to the API.
      // Once the closure compiler is able to type `assert` to return a truthy
      // type even when being used with modules, the conditionals should be
      // replaced with `assert` (b/138729777).
      if (currentWindow.tabs) {
        for (const tab of currentWindow.tabs) {
          if (tab) {
            this.onTabCreated_(tab, fragment);
          }
        }
      }
      this.tabsContainerElement_.appendChild(fragment);

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
    return /** @type {?TabElement} */ (this.tabsContainerElement_.querySelector(
        `tabstrip-tab[data-tab-id="${tabId}"]`));
  }

  /**
   * @param {!TabElement} tabElement
   * @param {number} index
   * @param {!Node=} opt_parent
   * @private
   */
  insertTabAt_(tabElement, index, opt_parent) {
    (opt_parent || this.tabsContainerElement_)
        .insertBefore(tabElement, this.tabsContainerElement_.children[index]);
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
        this.tabsContainerElement_.querySelector('tabstrip-tab[active]');
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
   * @param {!Node=} opt_parent
   * @private
   */
  onTabCreated_(tab, opt_parent) {
    if (tab.windowId !== this.windowId_) {
      return;
    }

    this.insertTabAt_(this.createTabElement_(tab), tab.index, opt_parent);
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
    }
  }
}

customElements.define('tabstrip-tab-list', TabListElement);
