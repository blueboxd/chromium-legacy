// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {getFavicon} from 'chrome://resources/js/icon.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {AlertIndicatorsElement} from './alert_indicators.js';
import {CustomElement} from './custom_element.js';
import {TabStripEmbedderProxy} from './tab_strip_embedder_proxy.js';
import {tabStripOptions} from './tab_strip_options.js';
import {TabData, TabNetworkState, TabsApiProxy} from './tabs_api_proxy.js';

export const DEFAULT_ANIMATION_DURATION = 125;

/**
 * @param {!TabData} tab
 * @return {string}
 */
function getAccessibleTitle(tab) {
  const tabTitle = tab.title;

  if (tab.crashed) {
    return loadTimeData.getStringF('tabCrashed', tabTitle);
  }

  if (tab.networkState === TabNetworkState.ERROR) {
    return loadTimeData.getStringF('tabNetworkError', tabTitle);
  }

  return tabTitle;
}

export class TabElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    this.alertIndicatorsEl_ = /** @type {!AlertIndicatorsElement} */
        (this.shadowRoot.querySelector('tabstrip-alert-indicators'));
    // Normally, custom elements will get upgraded automatically once added to
    // the DOM, but TabElement may need to update properties on
    // AlertIndicatorElement before this happens, so upgrade it manually.
    customElements.upgrade(this.alertIndicatorsEl_);

    /** @private {!HTMLElement} */
    this.closeButtonEl_ =
        /** @type {!HTMLElement} */ (this.shadowRoot.querySelector('#close'));
    this.closeButtonEl_.setAttribute(
        'aria-label', loadTimeData.getString('closeTab'));

    /** @private {!HTMLElement} */
    this.tabEl_ =
        /** @type {!HTMLElement} */ (this.shadowRoot.querySelector('#tab'));

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

    /** @private {!TabData} */
    this.tab_;

    /** @private {!TabsApiProxy} */
    this.tabsApi_ = TabsApiProxy.getInstance();

    /** @private {!TabStripEmbedderProxy} */
    this.embedderApi_ = TabStripEmbedderProxy.getInstance();

    /** @private {!HTMLElement} */
    this.titleTextEl_ = /** @type {!HTMLElement} */ (
        this.shadowRoot.querySelector('#titleText'));

    this.tabEl_.addEventListener('click', this.onClick_.bind(this));
    this.tabEl_.addEventListener('contextmenu', this.onContextMenu_.bind(this));
    this.closeButtonEl_.addEventListener('click', this.onClose_.bind(this));
  }

  /** @return {!TabData} */
  get tab() {
    return this.tab_;
  }

  /** @param {!TabData} tab */
  set tab(tab) {
    assert(this.tab_ !== tab);
    this.toggleAttribute('active', tab.active);
    this.tabEl_.setAttribute('aria-selected', tab.active.toString());
    this.toggleAttribute('hide-icon_', !tab.showIcon);
    this.toggleAttribute(
        'waiting_',
        !tab.shouldHideThrobber &&
            tab.networkState === TabNetworkState.WAITING);
    this.toggleAttribute(
        'loading_',
        !tab.shouldHideThrobber &&
            tab.networkState === TabNetworkState.LOADING);
    this.toggleAttribute('pinned', tab.pinned);
    this.toggleAttribute('blocked_', tab.blocked);
    this.setAttribute('draggable', true);
    this.toggleAttribute('crashed_', tab.crashed);

    if (!this.tab_ || this.tab_.title !== tab.title) {
      this.titleTextEl_.textContent = tab.title;
    }
    this.titleTextEl_.setAttribute('aria-label', getAccessibleTitle(tab));

    if (tab.networkState === TabNetworkState.WAITING ||
        (tab.networkState === TabNetworkState.LOADING &&
         tab.isDefaultFavicon)) {
      this.faviconEl_.style.backgroundImage = 'none';
    } else if (tab.favIconUrl) {
      this.faviconEl_.style.backgroundImage = `url(${tab.favIconUrl})`;
    } else {
      this.faviconEl_.style.backgroundImage = getFavicon('');
    }

    // Expose the ID to an attribute to allow easy querySelector use
    this.setAttribute('data-tab-id', tab.id);

    this.alertIndicatorsEl_.updateAlertStates(tab.alertStates)
        .then((alertIndicatorsCount) => {
          this.toggleAttribute('has-alert-states_', alertIndicatorsCount > 0);
        });

    if (!this.tab_ || this.tab_.id !== tab.id) {
      this.tabsApi_.trackThumbnailForTab(tab.id);
    }

    this.tab_ = Object.freeze(tab);
  }

  /** @return {!HTMLElement} */
  getDragImage() {
    return this.tabEl_;
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

    if (tabStripOptions.autoCloseEnabled) {
      this.embedderApi_.closeContainer();
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onContextMenu_(event) {
    event.preventDefault();

    if (!this.tab_) {
      return;
    }

    this.embedderApi_.showTabContextMenu(
        this.tab_.id, event.clientX, event.clientY);
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
    this.toggleAttribute('dragging_', dragging);
  }

  /**
   * @return {!Promise}
   */
  slideIn() {
    return new Promise(resolve => {
      const animation = this.animate(
          [
            {maxWidth: 0, opacity: 0},
            {maxWidth: 'var(--tabstrip-tab-width)', opacity: 1},
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
            {maxWidth: 'var(--tabstrip-tab-width)', opacity: 1},
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
