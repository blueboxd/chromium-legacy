// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './grid.js';
import './untrusted_iframe.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from './browser_proxy.js';

/** Element that lets the user configure the background. */
class CustomizeBackgroundsElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-backgrounds';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {newTabPage.mojom.BackgroundCollection} */
      selectedCollection: {
        notify: true,
        observer: 'onSelectedCollectionChange_',
        type: Object,
        value: null,
      },

      /** @private {!Array<!newTabPage.mojom.BackgroundCollection>} */
      collections_: Array,

      /** @private {!Array<!newTabPage.mojom.BackgroundImage>} */
      images_: Array,
    };
  }

  constructor() {
    super();
    BrowserProxy.getInstance().handler.getBackgroundCollections().then(
        ({collections}) => {
          this.collections_ = collections;
        });
  }

  /**
   * @param {!Event} e
   * @private
   */
  onCollectionClick_(e) {
    this.selectedCollection = this.$.collectionsRepeat.itemForElement(e.target);
  }

  /** @private */
  async onSelectedCollectionChange_() {
    this.images_ = [];
    if (!this.selectedCollection) {
      return;
    }
    const collectionId = this.selectedCollection.id;
    const {images} =
        await BrowserProxy.getInstance().handler.getBackgroundImages(
            collectionId);
    // We check the IDs match since the user may have already moved to a
    // different collection before the results come back.
    if (!this.selectedCollection ||
        this.selectedCollection.id !== collectionId) {
      return;
    }
    this.images_ = images;
  }
}

customElements.define(
    CustomizeBackgroundsElement.is, CustomizeBackgroundsElement);
