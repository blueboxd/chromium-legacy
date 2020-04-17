// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './grid.js';
import './mini_page.js';
import './untrusted_iframe.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from './browser_proxy.js';
import {BackgroundSelection, BackgroundSelectionType} from './customize_dialog.js';

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
      /** @type {!BackgroundSelection} */
      backgroundSelection: {
        type: Object,
        value: () => ({type: BackgroundSelectionType.NO_SELECTION}),
        notify: true,
      },

      /** @private {newTabPage.mojom.BackgroundCollection} */
      selectedCollection: {
        notify: true,
        observer: 'onSelectedCollectionChange_',
        type: Object,
        value: null,
      },

      /** @type {!newTabPage.mojom.Theme} */
      theme: Object,

      /** @private {!Array<!newTabPage.mojom.BackgroundCollection>} */
      collections_: Array,

      /** @private {!Array<!newTabPage.mojom.BackgroundImage>} */
      images_: Array,
    };
  }

  constructor() {
    super();
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    this.pageHandler_.getBackgroundCollections().then(({collections}) => {
      this.collections_ = collections;
    });
  }

  /**
   * @return {string}
   * @private
   */
  getCustomBackgroundClass_() {
    switch (this.backgroundSelection.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme && this.theme.backgroundImageUrl &&
                this.theme.backgroundImageUrl.url.startsWith(
                    'chrome-untrusted://new-tab-page/background.jpg') ?
            'selected' :
            '';
      default:
        return '';
    }
  }

  /**
   * @return {string}
   * @private
   */
  getNoBackgroundClass_() {
    switch (this.backgroundSelection.type) {
      case BackgroundSelectionType.NO_BACKGROUND:
        return 'selected';
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme && !this.theme.backgroundImageUrl &&
                !this.theme.dailyRefreshCollectionId ?
            'selected' :
            '';
      case BackgroundSelectionType.IMAGE:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getImageSelectedClass_(index) {
    const {url} = this.images_[index].imageUrl;
    switch (this.backgroundSelection.type) {
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection.image.imageUrl.url === url ?
            'selected' :
            '';
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme && this.theme.backgroundImageUrl &&
                this.theme.backgroundImageUrl.url === url &&
                !this.theme.dailyRefreshCollectionId ?
            'selected' :
            '';
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onCollectionClick_(e) {
    this.selectedCollection = this.$.collectionsRepeat.itemForElement(e.target);
  }

  /** @private */
  async onUploadFromDeviceClick_() {
    const {success} = await this.pageHandler_.chooseLocalCustomBackground();
    if (success) {
      // The theme update is asynchronous. Close the dialog and allow ntp-app
      // to update the |backgroundSelection|.
      this.dispatchEvent(new Event('close', {bubbles: true, composed: true}));
    }
  }

  /** @private */
  onDefaultClick_() {
    this.backgroundSelection = {type: BackgroundSelectionType.NO_BACKGROUND};
  }

  /**
   * @param {!Event} e
   * @private
   */
  onImageClick_(e) {
    this.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: this.$.imagesRepeat.itemForElement(e.target),
    };
  }

  /** @private */
  async onSelectedCollectionChange_() {
    this.images_ = [];
    if (!this.selectedCollection) {
      return;
    }
    const collectionId = this.selectedCollection.id;
    const {images} = await this.pageHandler_.getBackgroundImages(collectionId);
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
