// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './most_visited.js';
import './customize_dialog.js';
import './voice_search_overlay.js';
import './untrusted_iframe.js';
import './fakebox.js';
import './realbox.js';
import './logo.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {BackgroundSelection, BackgroundSelectionType} from './customize_dialog.js';
import {$$, hexColorToSkColor, skColorToRgba} from './utils.js';

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
      iframeOneGoogleBarEnabled_: {
        type: Boolean,
        value: () => {
          const params = new URLSearchParams(window.location.search);
          if (params.has('ogbinline')) {
            return false;
          }
          return loadTimeData.getBoolean('iframeOneGoogleBarEnabled') ||
              params.has('ogbiframe');
        },
        reflectToAttribute: true,
      },

      /** @private */
      oneGoogleBarIframePath_: {
        type: String,
        value: () => {
          const fromSearch = new URLSearchParams(window.location.search);
          const params = new URLSearchParams();
          params.set('ogdebencoded', btoa(fromSearch.get('ogdeb') || ''));
          return `one-google-bar?${params}`;
        },
      },

      /** @private */
      oneGoogleBarLoaded_: {
        observer: 'oneGoogleBarLoadedChange_',
        type: Boolean,
        value: false,
      },

      /** @private */
      oneGoogleBarDarkThemeEnabled_: {
        type: Boolean,
        computed: `computeOneGoogleBarDarkThemeEnabled_(oneGoogleBarLoaded_,
            theme_, backgroundSelection_)`,
        observer: 'onOneGoogleBarDarkThemeEnabledChange_',
      },

      /** @private */
      showIframedOneGoogleBar_: {
        type: Boolean,
        value: false,
        computed: `computeShowIframedOneGoogleBar_(iframeOneGoogleBarEnabled_,
            lazyRender_)`,
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

      /** @private */
      realboxEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('realboxEnabled'),
      },

      /**
       * If true, renders additional elements that were not deemed crucial to
       * to show up immediately on load.
       * @private
       */
      lazyRender_: Boolean,
    };
  }

  constructor() {
    performance.mark('app-creation-start');
    super();
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    /** @private {?number} */
    this.setThemeListenerId_ = null;
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
    this.loadOneGoogleBar_();

    /** @private {boolean} */
    this.shouldPrintPerformance_ =
        new URLSearchParams(location.search).has('print_perf');
    /** @private {number} */
    this.backgroundImageLoadStartEpoch_ = 0;
    /** @private {number} */
    this.backgroundImageLoadStart_ = 0;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener(theme => {
          performance.measure('theme-set');
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
        } else if (data.frameType === 'background-image') {
          this.handleBackgroundImageMessage_(data);
        }
      }
    });
    FocusOutlineManager.forDocument(document);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(assert(this.setThemeListenerId_));
    this.eventTracker_.removeAll();
  }

  /** @override */
  ready() {
    super.ready();
    // Let the browser breath and then render remaining elements.
    BrowserProxy.getInstance().waitForLazyRender().then(() => {
      this.lazyRender_ = true;
    });
    if (!this.shouldPrintPerformance_) {
      this.printPerformance_();
    }
    performance.measure('app-creation', 'app-creation-start');
  }

  /**
   * @return {boolean}
   * @private
   */
  computeOneGoogleBarDarkThemeEnabled_() {
    if (!this.theme_ || !this.oneGoogleBarLoaded_) {
      return false;
    }
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      case BackgroundSelectionType.NO_SELECTION:
      default:
        return this.theme_.isDark;
    }
  }

  /**
   * @return {!Promise}
   * @private
   */
  async loadOneGoogleBar_() {
    if (this.iframeOneGoogleBarEnabled_) {
      const oneGoogleBar = document.querySelector('#oneGoogleBar');
      oneGoogleBar.remove();
      return;
    }

    const {parts} =
        await BrowserProxy.getInstance().handler.getOneGoogleBarParts(
            (new URLSearchParams(window.location.search)).get('ogdeb') || '');
    if (!parts) {
      return;
    }

    const inHeadStyle = document.createElement('style');
    inHeadStyle.type = 'text/css';
    inHeadStyle.appendChild(document.createTextNode(parts.inHeadStyle));
    document.head.appendChild(inHeadStyle);

    const inHeadScript = document.createElement('script');
    inHeadScript.type = 'text/javascript';
    inHeadScript.appendChild(document.createTextNode(parts.inHeadScript));
    document.head.appendChild(inHeadScript);

    this.oneGoogleBarLoaded_ = true;
    const oneGoogleBar = document.querySelector('#oneGoogleBar');
    oneGoogleBar.innerHTML = parts.barHtml;

    const afterBarScript = document.createElement('script');
    afterBarScript.type = 'text/javascript';
    afterBarScript.appendChild(document.createTextNode(parts.afterBarScript));
    oneGoogleBar.parentNode.insertBefore(
        afterBarScript, oneGoogleBar.nextSibling);

    document.querySelector('#oneGoogleBarEndOfBody').innerHTML =
        parts.endOfBodyHtml;

    const endOfBodyScript = document.createElement('script');
    endOfBodyScript.type = 'text/javascript';
    endOfBodyScript.appendChild(document.createTextNode(parts.endOfBodyScript));
    document.body.appendChild(endOfBodyScript);
  }

  /** @private */
  async onOneGoogleBarDarkThemeEnabledChange_() {
    if (!this.oneGoogleBarLoaded_) {
      return;
    }
    if (this.iframeOneGoogleBarEnabled_) {
      $$(this, '#oneGoogleBar').postMessage({
        type: 'enableDarkTheme',
        enabled: this.oneGoogleBarDarkThemeEnabled_,
      });
      return;
    }
    const {gbar} = /** @type {{gbar}} */ (window);
    if (!gbar) {
      return;
    }
    const oneGoogleBar =
        await /** @type {!{a: {bf: function(): !Promise<{pc: !Function}>}}} */ (
            gbar)
            .a.bf();
    oneGoogleBar.pc.call(
        oneGoogleBar, this.oneGoogleBarDarkThemeEnabled_ ? 1 : 0);
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowIframedOneGoogleBar_() {
    return this.iframeOneGoogleBarEnabled_ && this.lazyRender_;
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
  rgbaOrInherit_(skColor) {
    return skColor ? skColorToRgba(skColor) : 'inherit';
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
    // change to the theme. Since |backgroundSelection_| has precedence over
    // the theme background, the |backgroundSelection_| needs to be reset when
    // the theme is updated. This is only necessary when the dialog is closed.
    // If the dialog is open, it will either commit the |backgroundSelection_|
    // or reset |backgroundSelection_| on cancel.
    //
    // Update after background image path is updated so the image is not shown
    // before the path is updated.
    if (!this.showCustomizeDialog_ &&
        this.backgroundSelection_.type !==
            BackgroundSelectionType.NO_SELECTION) {
      // Wait when local image is selected, then no background is previewed
      // followed by selecting a new local image. This avoids a flicker. The
      // iframe with the old image is shown briefly before it navigates to a new
      // iframe location, then fetches and renders the new local image.
      if (this.backgroundSelection_.type ===
          BackgroundSelectionType.NO_BACKGROUND) {
        setTimeout(() => {
          this.backgroundSelection_ = {
            type: BackgroundSelectionType.NO_SELECTION
          };
        }, 100);
      } else {
        this.backgroundSelection_ = {
          type: BackgroundSelectionType.NO_SELECTION
        };
      }
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
      this.backgroundImageLoadStartEpoch_ = BrowserProxy.getInstance().now();
      this.backgroundImageLoadStart_ = performance.now();
      this.$.backgroundImage.path = path;
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeDoodleAllowed_() {
    return !this.showBackgroundImage_ && this.theme_ &&
        this.theme_.type === newTabPage.mojom.ThemeType.DEFAULT &&
        !this.theme_.isDark;
  }

  /**
   * @return {skia.mojom.SkColor}
   * @private
   */
  computeLogoColor_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return hexColorToSkColor('#ffffff');
      case BackgroundSelectionType.NO_SELECTION:
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return this.theme_ &&
            (this.theme_.logoColor ||
             (this.theme_.isDark ? hexColorToSkColor('#ffffff') : null));
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeSingleColoredLogo_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.DAILY_REFRESH:
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.NO_SELECTION:
      default:
        return this.theme_ && (!!this.theme_.logoColor || this.theme_.isDark);
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
      $$(this, '#oneGoogleBar').style.zIndex = '1000';
    } else if (data.messageType === 'deactivate') {
      $$(this, '#oneGoogleBar').style.zIndex = '0';
    }
  }

  /**
   * @param {!Object} data
   * @private
   */
  handleBackgroundImageMessage_(data) {
    if (data.src === this.theme_.backgroundImageUrl.url &&
        data.messageType === 'loaded') {
      const duration = data.time - this.backgroundImageLoadStartEpoch_;
      this.printPerformanceDatum_(
          'background-image-load', this.backgroundImageLoadStart_, duration);
      this.printPerformanceDatum_(
          'background-image-loaded', this.backgroundImageLoadStart_ + duration,
          0);
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
            $$(this, '#promo').offsetTop;
        $$(this, '#promo').style.opacity = hidePromo ? 0 : 1;
      };
      this.eventTracker_.add(window, 'resize', onResize);
      onResize();
    }
  }

  /** @private */
  oneGoogleBarLoadedChange_() {
    if (this.oneGoogleBarLoaded_ && this.iframeOneGoogleBarEnabled_) {
      this.setupShortcutDragDropOneGoogleBarWorkaround_();
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
  setupShortcutDragDropOneGoogleBarWorkaround_() {
    const iframe = $$(this, '#oneGoogleBar');
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

  /** @private */
  printPerformanceDatum_(name, time, auxTime) {
    if (!this.shouldPrintPerformance_) {
      return;
    }
    if (!auxTime) {
      console.log(`${name}: ${time}`);
    } else {
      console.log(`${name}: ${time} (${auxTime})`);
    }
  }

  /**
   * Prints performance measurements to the console. Also, installs  performance
   * observer to continuously print performance measurements after.
   * @private
   */
  printPerformance_() {
    const entryTypes = ['paint', 'measure'];
    const log = (entry) => {
      this.printPerformanceDatum_(
          entry.name, entry.duration ? entry.duration : entry.startTime,
          entry.duration && entry.startTime ? entry.startTime : 0);
    };
    const observer = new PerformanceObserver(list => {
      list.getEntries().forEach(log);
    });
    observer.observe({entryTypes: entryTypes});
    for (const entry of performance.getEntries()) {
      if (!entryTypes.includes(entry.entryType)) {
        return;
      }
      log(entry);
    }
  }
}

customElements.define(AppElement.is, AppElement);
