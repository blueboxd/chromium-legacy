// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from './custom_element.js';
import {TabsApiProxy} from './tabs_api_proxy.js';

export class TabElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!HTMLElement} */
    this.closeButtonEl_ =
        /** @type {!HTMLElement} */ (this.shadowRoot.querySelector('#close'));

    /** @private {!Tab} */
    this.tab_;

    /** @private {!TabsApiProxy} */
    this.tabsApi_ = TabsApiProxy.getInstance();

    /** @private {!HTMLElement} */
    this.titleTextEl_ = /** @type {!HTMLElement} */ (
        this.shadowRoot.querySelector('#titleText'));

    this.closeButtonEl_.addEventListener('click', this.onClose_.bind(this));
  }

  /** @return {!Tab} */
  get tab() {
    return this.tab_;
  }

  /** @param {!Tab} tab */
  set tab(tab) {
    if (!this.tab_ || this.tab_.title !== tab.title) {
      this.titleTextEl_.textContent = tab.title;
    }

    // Expose the ID to an attribute to allow easy querySelector use
    this.setAttribute('data-tab-id', tab.id);

    this.tab_ = Object.freeze(tab);
  }

  /** @private */
  onClose_() {
    // There is no tab data and therefore nothing to close
    if (!this.tab_) {
      return;
    }

    this.tabsApi_.closeTab(this.tab_.id);
  }
}

customElements.define('tabstrip-tab', TabElement);
