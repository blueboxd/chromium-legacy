// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './most_visited.js';
import './customize_dialog.js';
import './voice_search_overlay.js';
import './untrusted_iframe.js';
import './fakebox.js';
import './logo.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {BackgroundSelection, BackgroundSelectionType} from './customize_dialog.js';
import {hexColorToSkColor, skColorToRgb} from './utils.js';

class AppElement extends PolymerElement {
  static get is() {
    return 'ntp-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      oneGoogleBarLoaded_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      promoLoaded_: {
        type: Boolean,
        value: false,
      },

      /** @private {!newTabPage.mojom.Theme} */
      theme_: {
        type: Object,
        observer: 'updateBackgroundImagePath_',
      },

      /** @private */
      showCustomizeDialog_: Boolean,

      /** @private */
      showVoiceSearchOverlay_: Boolean,

      /** @private */
      showBackgroundImage_: {
        computed: 'computeShowBackgroundImage_(theme_, backgroundSelection_)',
        reflectToAttribute: true,
        type: Boolean,
      },

      /** @private {!BackgroundSelection} */
      backgroundSelection_: {
        type: Object,
        value: () => ({type: BackgroundSelectionType.NO_SELECTION}),
        observer: 'updateBackgroundImagePath_',
      },

      /** @private */
      backgroundImageAttribution1_: {
        type: String,
        computed: `computeBackgroundImageAttribution1_(theme_,
            backgroundSelection_)`,
      },

      /** @private */
      backgroundImageAttribution2_: {
        type: String,
        computed: `computeBackgroundImageAttribution2_(theme_,
            backgroundSelection_)`,
      },

      /** @private */
      backgroundImageAttributionUrl_: {
        type: String,
        computed: `computeBackgroundImageAttributionUrl_(theme_,
            backgroundSelection_)`,
      },

      /** @private */
      doodleAllowed_: {
        computed: 'computeDoodleAllowed_(showBackgroundImage_, theme_)',
        type: Boolean,
      },

      /** @private */
      logoColor_: {
        type: String,
        computed: 'computeLogoColor_(theme_, backgroundSelection_)',
      },

      /** @private */
      singleColoredLogo_: {
        computed: 'computeSingleColoredLogo_(theme_, backgroundSelection_)',
        type: Boolean,
      },
    };
  }

  constructor() {
    super();
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    /** @private {?number} */
    this.setThemeListenerId_ = null;
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener(theme => {
          this.theme_ = theme;
        });
    this.eventTracker_.add(window, 'message', ({data}) => {
      // Something in OneGoogleBar is sending a message that is received here.
      // Need to ignore it.
      if (typeof data !== 'object') {
        return;
      }
      if ('frameType' in data) {
        if (data.frameType === 'promo') {
          this.handlePromoMessage_(data);
        } else if (data.frameType === 'one-google-bar') {
          this.handleOneGoogleBarMessage_(data);
        }
      }
    });
    this.setupShortcutDragDropOgbWorkaround_();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(assert(this.setThemeListenerId_));
    this.eventTracker_.removeAll();
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttribution1_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttribution1 || '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attribution1;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttribution2_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttribution2 || '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attribution2;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttributionUrl_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttributionUrl ?
            this.theme_.backgroundImageAttributionUrl.url :
            '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attributionUrl.url;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /** @private */
  onVoiceSearchClick_() {
    this.showVoiceSearchOverlay_ = true;
  }

  /** @private */
  onCustomizeClick_() {
    this.showCustomizeDialog_ = true;
  }

  /** @private */
  onCustomizeDialogClose_() {
    this.showCustomizeDialog_ = false;
  }

  /** @private */
  onVoiceSearchOverlayClose_() {
    this.showVoiceSearchOverlay_ = false;
  }

  /**
   * @param {skia.mojom.SkColor} skColor
   * @return {string}
   * @private
   */
  rgbOrInherit_(skColor) {
    return skColor ? skColorToRgb(skColor) : 'inherit';
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowBackgroundImage_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return !!this.theme_ && !!this.theme_.backgroundImageUrl;
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return false;
    }
  }

  /**
   * Set the #backgroundImage |path| only when different and non-empty. Reset
   * the customize dialog background selection if the dialog is closed.
   *
   * The ntp-untrusted-iframe |path| is set directly. When using a data binding
   * instead, the quick updates to the |path| result in iframe loading an error
   * page.
   * @private
   */
  updateBackgroundImagePath_() {
    // The |backgroundSelection_| is retained after the dialog commits the
    // change to the theme. Since |backgroundSelection_| has precendence over
    // the theme background, the |backgroundSelection_| needs to be reset when
    // the theme is updated. This is only necessary when the dialog is closed.
    // If the dialog is open, it will either commit the |backgroundSelection_|
    // or reset |backgroundSelection_| on cancel.
    if (!this.showCustomizeDialog_ &&
        this.backgroundSelection_.type !==
            BackgroundSelectionType.NO_SELECTION) {
      this.backgroundSelection_ = {type: BackgroundSelectionType.NO_SELECTION};
    }
    let path;
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        path = this.theme_ && this.theme_.backgroundImageUrl &&
            `background_image?${this.theme_.backgroundImageUrl.url}`;
        break;
      case BackgroundSelectionType.IMAGE:
        path =
            `background_image?${this.backgroundSelection_.image.imageUrl.url}`;
        break;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        path = '';
    }
    if (path && this.$.backgroundImage.path !== path) {
      this.$.backgroundImage.path = path;
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeDoodleAllowed_() {
    return !this.showBackgroundImage_ &&
        (!this.theme_ ||
         this.theme_.type === newTabPage.mojom.ThemeType.DEFAULT);
  }

  /**
   * @return {skia.mojom.SkColor}
   * @private
   */
  computeLogoColor_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.logoColor || null;
      case BackgroundSelectionType.IMAGE:
        return hexColorToSkColor('#ffffff');
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return null;
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeSingleColoredLogo_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && !!this.theme_.logoColor;
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return false;
    }
  }

  /**
   * Handles messages from the OneGoogleBar iframe. The messages that are
   * handled include show bar on load and activate/deactivate.
   * The activate/deactivate controls if the OneGoogleBar is layered on top of
   * #content. This would happen when OneGoogleBar has an overlay open.
   * @param {!Object} data
   * @private
   */
  handleOneGoogleBarMessage_(data) {
    if (data.messageType === 'loaded') {
      this.oneGoogleBarLoaded_ = true;
    } else if (data.messageType === 'activate') {
      this.$.oneGoogleBar.style.zIndex = '1000';
    } else if (data.messageType === 'deactivate') {
      this.$.oneGoogleBar.style.zIndex = '0';
    }
  }

  /**
   * Handle messages from promo iframe. This shows the promo on load and sets
   * up the show/hide logic (in case there is an overlap with most-visited
   * tiles).
   * @param {!Object} data
   * @private
   */
  handlePromoMessage_(data) {
    if (data.messageType === 'loaded') {
      this.promoLoaded_ = true;
      const onResize = () => {
        const hidePromo = this.$.mostVisited.getBoundingClientRect().bottom >=
            this.$.promo.offsetTop;
        this.$.promo.style.opacity = hidePromo ? 0 : 1;
      };
      this.eventTracker_.add(window, 'resize', onResize);
      onResize();
    }
  }

  /**
   * During a shortcut drag, an iframe behind ntp-most-visited will prevent
   * 'dragover' events from firing. To workaround this, 'pointer-events: none'
   * can be set on the iframe. When doing this after the 'dragstart' event is
   * fired is too late. We can instead set 'pointer-events: none' when the
   * pointer enters ntp-most-visited.
   *
   * 'pointerenter' and pointerleave' events fire during drag. The iframe
   * 'pointer-events' needs to be reset to the original value when 'dragend'
   * fires if the pointer has left ntp-most-visited.
   * @private
   */
  setupShortcutDragDropOgbWorkaround_() {
    const iframe = this.$.oneGoogleBar;
    let resetAtDragEnd = false;
    let dragging = false;
    let originalPointerEvents;
    this.eventTracker_.add(this.$.mostVisited, 'pointerenter', () => {
      if (dragging) {
        resetAtDragEnd = false;
        return;
      }
      originalPointerEvents = getComputedStyle(iframe).pointerEvents;
      iframe.style.pointerEvents = 'none';
    });
    this.eventTracker_.add(this.$.mostVisited, 'pointerleave', () => {
      if (dragging) {
        resetAtDragEnd = true;
        return;
      }
      iframe.style.pointerEvents = originalPointerEvents;
    });
    this.eventTracker_.add(this.$.mostVisited, 'dragstart', () => {
      dragging = true;
    });
    this.eventTracker_.add(this.$.mostVisited, 'dragend', () => {
      dragging = false;
      if (resetAtDragEnd) {
        resetAtDragEnd = false;
        iframe.style.pointerEvents = originalPointerEvents;
      }
    });
  }
}

customElements.define(AppElement.is, AppElement);
