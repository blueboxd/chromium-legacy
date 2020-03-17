// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './most_visited.js';
import './customize_dialog.js';
import './voice_search_overlay.js';
import './untrusted_iframe.js';
import './fakebox.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {skColorToRgb} from './utils.js';

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
      promoLoaded_: {
        type: Boolean,
        value: false,
      },

      /** @private {!newTabPage.mojom.Theme} */
      theme_: Object,

      /** @private */
      showCustomizeDialog_: Boolean,

      /** @private */
      showVoiceSearchOverlay_: Boolean,

      /** @private */
      showBackgroundImage_: {
        computed: 'computeShowBackgroundImage_(theme_)',
        reflectToAttribute: true,
        type: Boolean,
      },

      /** @private */
      backgroundImagePath_: {
        computed: 'computeBackgroundImagePath_(theme_)',
        type: String,
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
      if ('frameType' in data && data.frameType === 'promo' &&
          data.messageType === 'loaded') {
        this.promoLoaded_ = true;
        const onResize = () => {
          const hidePromo = this.$.mostVisited.getBoundingClientRect().bottom >=
              this.$.promo.offsetTop;
          this.$.promo.style.opacity = hidePromo ? 0 : 1;
        };
        this.eventTracker_.add(window, 'resize', onResize);
        onResize();
      }
    });
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(assert(this.setThemeListenerId_));
    this.eventTracker_.removeAll();
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
    return !!this.theme_ && !!this.theme_.backgroundImageUrl;
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImagePath_() {
    if (!this.theme_ || !this.theme_.backgroundImageUrl) {
      return '';
    }
    return `image?${this.theme_.backgroundImageUrl.url}`;
  }
}

customElements.define(AppElement.is, AppElement);
