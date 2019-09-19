// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFavicon, getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';

import {CustomElement} from './custom_element.js';
import {TabsApiProxy} from './tabs_api_proxy.js';

export const DEFAULT_ANIMATION_DURATION = 125;

export class TabElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!HTMLElement} */
    this.closeButtonEl_ =
        /** @type {!HTMLElement} */ (this.shadowRoot.querySelector('#close'));

    /** @private {!HTMLElement} */
    this.dragImage_ =
        /** @type {!HTMLElement} */ (
            this.shadowRoot.querySelector('#dragImage'));

    /** @private {!HTMLElement} */
    this.faviconEl_ =
        /** @type {!HTMLElement} */ (this.shadowRoot.querySelector('#favicon'));

    /** @private {!HTMLElement} */
    this.thumbnailContainer_ =
        /** @type {!HTMLElement} */ (
            this.shadowRoot.querySelector('#thumbnail'));

    /** @private {!Image} */
    this.thumbnail_ =
        /** @type {!Image} */ (this.shadowRoot.querySelector('#thumbnailImg'));

    /** @private {!Tab} */
    this.tab_;

    /** @private {!TabsApiProxy} */
    this.tabsApi_ = TabsApiProxy.getInstance();

    /** @private {!HTMLElement} */
    this.titleTextEl_ = /** @type {!HTMLElement} */ (
        this.shadowRoot.querySelector('#titleText'));

    this.addEventListener('click', this.onClick_.bind(this));
    this.closeButtonEl_.addEventListener('click', this.onClose_.bind(this));
  }

  connectedCallback() {
    this.setAttribute('draggable', 'true');
  }

  /** @return {!Tab} */
  get tab() {
    return this.tab_;
  }

  /** @param {!Tab} tab */
  set tab(tab) {
    this.toggleAttribute('active', tab.active);
    this.toggleAttribute('pinned', tab.pinned);

    if (!this.tab_ || this.tab_.title !== tab.title) {
      this.titleTextEl_.textContent = tab.title;
    }

    if (tab.favIconUrl &&
        (!this.tab_ || this.tab_.favIconUrl !== tab.favIconUrl)) {
      this.faviconEl_.style.backgroundImage = getFavicon(tab.favIconUrl);
    } else if (!this.tab_ || this.tab_.url !== tab.url) {
      this.faviconEl_.style.backgroundImage =
          getFaviconForPageURL(tab.url, false);
    }

    // Expose the ID to an attribute to allow easy querySelector use
    this.setAttribute('data-tab-id', tab.id);

    if (!this.tab_ || this.tab_.id !== tab.id) {
      this.tabsApi_.trackThumbnailForTab(tab.id);
    }

    this.tab_ = Object.freeze(tab);
  }

  /**
   * @return {!HTMLElement}
   */
  getDragImage() {
    return this.dragImage_;
  }

  /**
   * @param {string} imgData
   */
  updateThumbnail(imgData) {
    this.thumbnail_.src = imgData;
  }

  /** @private */
  onClick_() {
    if (!this.tab_) {
      return;
    }

    this.tabsApi_.activateTab(this.tab_.id);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onClose_(event) {
    if (!this.tab_) {
      return;
    }

    event.stopPropagation();
    this.tabsApi_.closeTab(this.tab_.id);
  }

  /**
   * @param {boolean} dragging
   */
  setDragging(dragging) {
    this.toggleAttribute('dragging', dragging);
  }

  /**
   * @return {!Promise}
   */
  slideIn() {
    return new Promise(resolve => {
      const animation = this.animate(
          [
            {maxWidth: 0, opacity: 0},
            {maxWidth: '280px', opacity: 1},
          ],
          {
            duration: DEFAULT_ANIMATION_DURATION,
            fill: 'forwards',
          });
      animation.onfinish = resolve;
    });
  }

  /**
   * @return {!Promise}
   */
  slideOut() {
    return new Promise(resolve => {
      const animation = this.animate(
          [
            {maxWidth: '280px', opacity: 1},
            {maxWidth: 0, opacity: 0},
          ],
          {
            duration: DEFAULT_ANIMATION_DURATION,
            fill: 'forwards',
          });
      animation.onfinish = () => {
        this.remove();
        resolve();
      };
    });
  }
}

customElements.define('tabstrip-tab', TabElement);
