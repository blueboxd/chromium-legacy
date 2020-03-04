// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './customize_backgrounds.js';
import './customize_shortcuts.js';
import './customize_themes.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {createScrollBorders} from './utils.js';

/**
 * Dialog that lets the user customize the NTP such as the background color or
 * image.
 */
class CustomizeDialogElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!newTabPage.mojom.Theme} */
      theme: Object,

      /** @private */
      selectedPage_: {
        type: String,
        value: 'backgrounds',
        observer: 'onSelectedPageChange_',
      },

      /** @private {newTabPage.mojom.BackgroundCollection} */
      selectedCollection_: Object,

      /** @private */
      showTitleNavigation_: {
        type: Boolean,
        computed:
            'computeShowTitleNavigation_(selectedPage_, selectedCollection_)',
        value: false,
      },
    };
  }

  constructor() {
    super();
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    /** @private {!Array<!IntersectionObserver>} */
    this.intersectionObservers_ = [];
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.intersectionObservers_.forEach(observer => {
      observer.disconnect();
    });
    this.intersectionObservers_ = [];
  }

  /** @override */
  ready() {
    super.ready();
    this.intersectionObservers_ = [
      this.$.menu,
      this.$.pages,
    ].map(createScrollBorders);
  }

  /** @private */
  onCancelClick_() {
    this.pageHandler_.revertThemeChanges();
    this.$.dialog.cancel();
  }

  /** @private */
  onDoneClick_() {
    this.pageHandler_.confirmThemeChanges();
    this.shadowRoot.querySelector('ntp-customize-shortcuts').apply();
    this.$.dialog.close();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onMenuItemKeyDown_(e) {
    if (!['Enter', ' '].includes(e.key)) {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    this.selectedPage_ = e.target.getAttribute('page-name');
  }

  /** @private */
  onSelectedPageChange_() {
    this.$.pages.scrollTop = 0;
  }

  /** @private */
  computeShowTitleNavigation_() {
    return this.selectedPage_ === 'backgrounds' && this.selectedCollection_;
  }

  /** @private */
  onBackClick_() {
    this.selectedCollection_ = null;
  }
}

customElements.define(CustomizeDialogElement.is, CustomizeDialogElement);
