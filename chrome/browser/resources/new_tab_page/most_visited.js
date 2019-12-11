// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './strings.m.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Debouncer, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * @enum {number}
 * @const
 */
const ScreenWidth = {
  NARROW: 0,
  MEDIUM: 1,
  WIDE: 2,
};

/**
 * @param {string} msgId
 * @private
 */
function toast(msgId) {
  getToastManager().show(loadTimeData.getString(msgId));
}

class MostVisitedElement extends PolymerElement {
  static get is() {
    return 'ntp-most-visited';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      columnCount_: {
        type: Boolean,
        computed: 'computeColumnCount_(tiles_, screenWidth_)',
      },

      /** @private */
      dialogTileTitle_: String,

      /** @private */
      dialogTileUrl_: String,

      /** @private */
      dialogTileUrlInvalid_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      dialogTitle_: String,

      /** @private */
      isRtl_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @private */
      showAdd_: {
        type: Boolean,
        computed: 'computeShowAdd_(tiles_, columnCount_)',
      },

      /** @private {!ScreenWidth} */
      screenWidth_: Number,

      /** @private {!Array<!newTabPage.mojom.MostVisitedTile>} */
      tiles_: Array,
    };
  }

  constructor() {
    super();
    /** @private {boolean} */
    this.adding_ = false;
    const {callbackRouter, handler} = BrowserProxy.getInstance();
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = callbackRouter;
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = handler;
    /** @private {?Debouncer} */
    this.resizeDebouncer_ = null;
    /** @private {?number} */
    this.setMostVisitedTilesListenerId_ = null;
    /** @private {number} */
    this.targetIndex_ = -1;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    /** @private {boolean} */
    this.isRtl_ = window.getComputedStyle(this)['direction'] === 'rtl';
    this.setMostVisitedTilesListenerId_ =
        this.callbackRouter_.setMostVisitedTiles.addListener(tiles => {
          this.tiles_ = tiles.length > 10 ? tiles.slice(0, 10) : tiles;
        });
    FocusOutlineManager.forDocument(document);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.setMostVisitedTilesListenerId_));
    this.mediaListenerWideWidth_.removeListener(
        assert(this.boundOnWidthChange_));
    this.mediaListenerMediumWidth_.removeListener(
        assert(this.boundOnWidthChange_));
  }

  /** @override */
  ready() {
    super.ready();

    /** @private {!Function} */
    this.boundOnWidthChange_ = this.updateScreenWidth_.bind(this);
    /** @private {!MediaQueryList} */
    this.mediaListenerWideWidth_ = window.matchMedia('(min-width: 672px)');
    this.mediaListenerWideWidth_.addListener(this.boundOnWidthChange_);
    /** @private {!MediaQueryList} */
    this.mediaListenerMediumWidth_ = window.matchMedia('(min-width: 560px)');
    this.mediaListenerMediumWidth_.addListener(this.boundOnWidthChange_);
    this.updateScreenWidth_();
  }

  /**
   * @return {number}
   * @private
   */
  computeColumnCount_() {
    let maxColumns = 3;
    if (this.screenWidth_ === ScreenWidth.WIDE) {
      maxColumns = 5;
    } else if (this.screenWidth_ === ScreenWidth.MEDIUM) {
      maxColumns = 4;
    }

    // Add 1 for the add shortcut.
    const tileCount = (this.tiles_ ? this.tiles_.length : 0) + 1;
    const columnCount = tileCount <= maxColumns ?
        tileCount :
        Math.min(maxColumns, Math.ceil(tileCount / 2));
    return columnCount;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowAdd_() {
    return this.tiles_ && this.tiles_.length < this.columnCount_ * 2;
  }

  /**
   * @param {!url.mojom.Url} url
   * @return {string}
   * @private
   */
  getFaviconUrl_(url) {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '24');
    faviconUrl.searchParams.set('show_fallback_monogram', '');
    faviconUrl.searchParams.set('page_url', url.url);
    return faviconUrl.href;
  }

  /**
   * @param {number} index
   * @return {boolean}
   * @private
   */
  isHidden_(index) {
    return index >= this.columnCount_ * 2;
  }

  /** @private */
  onAdd_() {
    this.dialogTitle_ = loadTimeData.getString('addLinkTitle');
    this.dialogTileTitle_ = '';
    this.dialogTileUrl_ = '';
    this.dialogTileUrlInvalid_ = false;
    this.adding_ = true;
    this.$.dialog.showModal();
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onAddShortcutKeydown_(e) {
    if (e.altKey || e.shiftKey || e.metaKey || e.ctrlKey) {
      return;
    }
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.onAdd_();
    }
  }

  /** @private */
  onDialogCancel_() {
    this.targetIndex_ = -1;
    this.$.dialog.cancel();
  }

  /** @private */
  onDialogClose_() {
    if (this.adding_) {
      this.$.addShortcut.focus();
    }
    this.adding_ = false;
  }

  /** @private */
  onEdit_() {
    this.$.actionMenu.close();
    this.dialogTitle_ = loadTimeData.getString('editLinkTitle');
    const {title, url} = this.tiles_[this.targetIndex_];
    this.dialogTileTitle_ = title;
    this.dialogTileUrl_ = url.url;
    this.dialogTileUrlInvalid_ = false;
    this.$.dialog.showModal();
  }

  /** @private */
  async onRemove_() {
    this.$.actionMenu.close();
    const {title, url} = this.tiles_[this.targetIndex_];
    const {success} = await this.pageHandler_.deleteMostVisitedTile(url);
    toast(success ? 'linkRemove' : 'linkCantRemove');
    const focusIndex =
        Math.min(this.tiles_.length, Math.max(0, this.targetIndex_));
    this.shadowRoot.querySelectorAll('.tile, #addShortcut')[focusIndex].focus();
    this.targetIndex_ = -1;
  }

  /** @private */
  async onSave_() {
    let newUrl;
    try {
      newUrl = new URL(
          this.dialogTileUrl_.includes('://') ?
              this.dialogTileUrl_ :
              `https://${this.dialogTileUrl_}/`);
      if (!['http:', 'https:'].includes(newUrl.protocol)) {
        throw new Error();
      }
    } catch (e) {
      this.dialogTileUrlInvalid_ = true;
      return;
    }

    this.dialogTileUrlInvalid_ = false;

    this.$.dialog.close();
    let newTitle = this.dialogTileTitle_.trim();
    if (newTitle.length === 0) {
      newTitle = this.dialogTileUrl_;
    }
    if (this.adding_) {
      const {success} = await this.pageHandler_.addMostVisitedTile(
          {url: newUrl.href}, newTitle);
      toast(success ? 'linkAddedMsg' : 'linkCantCreate');
    } else {
      const {url, title} = this.tiles_[this.targetIndex_];
      if (url.url !== newUrl.href || title !== newTitle) {
        const {success} = await this.pageHandler_.updateMostVisitedTile(
            url, {url: newUrl.href}, newTitle);
        toast(success ? 'linkEditedMsg' : 'linkCantEdit');
      }
      this.targetIndex_ = -1;
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onTileActionMenu_(e) {
    e.preventDefault();
    this.targetIndex_ =
        this.$.tiles.modelForElement(e.target.parentElement).index;
    this.$.actionMenu.showAt(e.target);
  }

  /** @private */
  updateScreenWidth_() {
    if (this.mediaListenerWideWidth_.matches) {
      this.screenWidth_ = ScreenWidth.WIDE;
    } else if (this.mediaListenerMediumWidth_.matches) {
      this.screenWidth_ = ScreenWidth.MEDIUM;
    } else {
      this.screenWidth_ = ScreenWidth.NARROW;
    }
  }
}

customElements.define(MostVisitedElement.is, MostVisitedElement);
