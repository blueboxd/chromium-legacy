// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './strings.m.js';

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
        computed: `computeColumnCount_(tiles_, screenWidth_, maxTiles_,
            visible_, showAdd_)`,
      },

      /** @private */
      customLinksEnabled_: Boolean,

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
      maxTiles_: {
        type: Number,
        computed: 'computeMaxTiles_(visible_, customLinksEnabled_)',
      },

      /** @private */
      showAdd_: {
        type: Boolean,
        computed: 'computeShowAdd_(tiles_, maxTiles_, customLinksEnabled_)',
      },

      /** @private */
      showToastButtons_: Boolean,

      /** @private {!ScreenWidth} */
      screenWidth_: Number,

      /** @private {!Array<!newTabPage.mojom.MostVisitedTile>} */
      tiles_: Array,

      /** @private */
      toastContent_: String,

      /** @private */
      visible_: Boolean,
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
    this.setCustomLinksEnabledListenerId_ = null;
    /** @private {?number} */
    this.setMostVisitedInfoListenerId_ = null;
    /** @private {?number} */
    this.setMostVisitedVisibleListenerId_ = null;
    /** @private {number} */
    this.actionMenuTargetIndex_ = -1;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    /** @private {boolean} */
    this.isRtl_ = window.getComputedStyle(this)['direction'] === 'rtl';
    this.setCustomLinksEnabledListenerId_ =
        this.callbackRouter_.setCustomLinksEnabled.addListener(enabled => {
          this.customLinksEnabled_ = enabled;
        });
    this.setMostVisitedInfoListenerId_ =
        this.callbackRouter_.setMostVisitedInfo.addListener(info => {
          this.visible_ = info.visible;
          this.customLinksEnabled_ = info.customLinksEnabled;
          this.tiles_ = info.tiles.slice(0, 10);
        });
    this.setMostVisitedVisibleListenerId_ =
        this.callbackRouter_.setMostVisitedVisible.addListener(visible => {
          this.visible_ = visible;
        });
    FocusOutlineManager.forDocument(document);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.setCustomLinksEnabledListenerId_));
    this.callbackRouter_.removeListener(
        assert(this.setMostVisitedInfoListenerId_));
    this.callbackRouter_.removeListener(
        assert(this.setMostVisitedVisibleListenerId_));
    this.mediaListenerWideWidth_.removeListener(
        assert(this.boundOnWidthChange_));
    this.mediaListenerMediumWidth_.removeListener(
        assert(this.boundOnWidthChange_));
    this.ownerDocument.removeEventListener(
        'keydown', this.boundOnDocumentKeyDown_);
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
    /** @private {!function(Event)} */
    this.boundOnDocumentKeyDown_ = e =>
        this.onDocumentKeyDown_(/** @type {!KeyboardEvent} */ (e));
    this.ownerDocument.addEventListener(
        'keydown', this.boundOnDocumentKeyDown_);
  }

  /**
   * @return {number}
   * @private
   */
  computeColumnCount_() {
    if (!this.visible_) {
      return 0;
    }

    let maxColumns = 3;
    if (this.screenWidth_ === ScreenWidth.WIDE) {
      maxColumns = 5;
    } else if (this.screenWidth_ === ScreenWidth.MEDIUM) {
      maxColumns = 4;
    }


    const tileCount = Math.min(
        this.maxTiles_,
        (this.tiles_ ? this.tiles_.length : 0) + (this.showAdd_ ? 1 : 0));
    const columnCount = tileCount <= maxColumns ?
        tileCount :
        Math.min(maxColumns, Math.ceil(tileCount / 2));
    return columnCount || 3;
  }

  /**
   * @return {number}
   * @private
   */
  computeMaxTiles_() {
    return !this.visible_ ? 0 : (this.customLinksEnabled_ ? 10 : 8);
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowAdd_() {
    return this.customLinksEnabled_ && this.tiles_ &&
        this.tiles_.length < this.maxTiles_;
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
   * @return {string}
   * @private
   */
  getRestoreButtonText_() {
    return loadTimeData.getString(
        this.customLinksEnabled_ ? 'restoreDefaultLinks' :
                                   'restoreThumbnailsShort');
  }

  /**
   * @return {string}
   * @private
   */
  getTileIconButtonIcon_() {
    return this.customLinksEnabled_ ? 'icon-more-vert' : 'icon-clear';
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
  onAddShortcutKeyDown_(e) {
    if (e.altKey || e.shiftKey || e.metaKey || e.ctrlKey) {
      return;
    }
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.onAdd_();
    }

    if (!this.tiles_ || this.tiles_.length === 0) {
      return;
    }
    const backKey = this.isRtl_ ? 'ArrowRight' : 'ArrowLeft';
    if (e.key === backKey || e.key == 'ArrowUp') {
      this.tileFocus_(this.tiles_.length - 1);
    }
  }

  /** @private */
  onDialogCancel_() {
    this.actionMenuTargetIndex_ = -1;
    this.$.dialog.cancel();
  }

  /** @private */
  onDialogClose_() {
    if (this.adding_) {
      this.$.addShortcut.focus();
    }
    this.adding_ = false;
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onDocumentKeyDown_(e) {
    if (e.altKey || e.shiftKey) {
      return;
    }

    const modifier = isMac ? e.metaKey && !e.ctrlKey : e.ctrlKey && !e.metaKey;
    if (modifier && e.key == 'z') {
      e.preventDefault();
      this.pageHandler_.undoMostVisitedTileAction();
    }
  }

  /** @private */
  onEdit_() {
    this.$.actionMenu.close();
    this.dialogTitle_ = loadTimeData.getString('editLinkTitle');
    const {title, url} = this.tiles_[this.actionMenuTargetIndex_];
    this.dialogTileTitle_ = title;
    this.dialogTileUrl_ = url.url;
    this.dialogTileUrlInvalid_ = false;
    this.$.dialog.showModal();
  }

  /** @private */
  onRestoreDefaultsClick_() {
    this.$.toast.hide();
    this.pageHandler_.restoreMostVisitedDefaults();
  }

  /** @private */
  async onRemove_() {
    this.$.actionMenu.close();
    await this.tileRemove_(this.actionMenuTargetIndex_);
    this.actionMenuTargetIndex_ = -1;
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
      this.toast_(success ? 'linkAddedMsg' : 'linkCantCreate', success);
    } else {
      const {url, title} = this.tiles_[this.actionMenuTargetIndex_];
      if (url.url !== newUrl.href || title !== newTitle) {
        const {success} = await this.pageHandler_.updateMostVisitedTile(
            url, {url: newUrl.href}, newTitle);
        this.toast_(success ? 'linkEditedMsg' : 'linkCantEdit', success);
      }
      this.actionMenuTargetIndex_ = -1;
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onTileIconButtonClick_(e) {
    e.preventDefault();
    const {index} = this.$.tiles.modelForElement(e.target.parentElement);
    if (this.customLinksEnabled_) {
      this.actionMenuTargetIndex_ = index;
      this.$.actionMenu.showAt(e.target);
    } else {
      this.tileRemove_(index);
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onTileKeyDown_(e) {
    if (e.altKey || e.shiftKey || e.metaKey || e.ctrlKey) {
      return;
    }

    if (e.key != 'ArrowLeft' && e.key != 'ArrowRight' && e.key != 'ArrowUp' &&
        e.key != 'ArrowDown' && e.key != 'Delete') {
      return;
    }

    const {index} = this.$.tiles.modelForElement(e.target);
    if (e.key == 'Delete') {
      this.tileRemove_(index);
      return;
    }

    const advanceKey = this.isRtl_ ? 'ArrowLeft' : 'ArrowRight';
    const delta = (e.key == advanceKey || e.key == 'ArrowDown') ? 1 : -1;
    this.tileFocus_(index + delta);
  }

  /** @private */
  onUndoClick_() {
    this.$.toast.hide();
    this.pageHandler_.undoMostVisitedTileAction();
  }

  /**
   * @param {number} index
   * @private
   */
  tileFocus_(index) {
    const tileCount = Math.min(
        this.columnCount_ * 2, this.tiles_.length + (this.showAdd_ ? 1 : 0));
    if (tileCount === 0) {
      return;
    }
    const focusIndex = Math.min(Math.max(0, index), tileCount - 1);
    this.shadowRoot.querySelectorAll('.tile, #addShortcut')[focusIndex].focus();
  }

  /**
   * @param {string} msgId
   * @param {boolean} showButtons
   * @private
   */
  toast_(msgId, showButtons) {
    this.toastContent_ = loadTimeData.getString(msgId);
    this.showToastButtons_ = showButtons;
    this.$.toast.show();
  }

  /**
   * @param {number} index
   * @return {!Promise}
   * @private
   */
  async tileRemove_(index) {
    const {title, url} = this.tiles_[index];
    const {success} = await this.pageHandler_.deleteMostVisitedTile(url);
    this.toast_(success ? 'linkRemove' : 'linkCantRemove', success);
    this.tileFocus_(index);
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
