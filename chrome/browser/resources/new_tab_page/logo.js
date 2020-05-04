// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './untrusted_iframe.js';
import './doodle_share_dialog.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from './browser_proxy.js';
import {skColorToRgba} from './utils.js';

// Shows the Google logo or a doodle if available.
class LogoElement extends PolymerElement {
  static get is() {
    return 'ntp-logo';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * If true displays doodle if one is available instead of Google logo.
       * @type {boolean}
       */
      doodleAllowed: {
        reflectToAttribute: true,
        type: Boolean,
        value: true,
      },

      /**
       * If true displays the Google logo single-colored.
       * @type {boolean}
       */
      singleColored: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      /** @private */
      loaded_: Boolean,

      /** @private {newTabPage.mojom.Doodle} */
      doodle_: Object,

      /** @private */
      canShowDoodle_: {
        computed: 'computeCanShowDoodle_(doodle_)',
        type: Boolean,
      },

      /** @private */
      showLogo_: {
        computed: 'computeShowLogo_(doodleAllowed, loaded_, canShowDoodle_)',
        type: Boolean,
      },

      /** @private */
      showDoodle_: {
        computed: 'computeShowDoodle_(doodleAllowed, loaded_, canShowDoodle_)',
        type: Boolean,
      },

      /** @private */
      imageUrl_: {
        computed: 'computeImageUrl_(doodle_)',
        type: String,
      },

      /** @private */
      showAnimation_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      animationUrl_: {
        computed: 'computeAnimationUrl_(doodle_)',
        type: String,
      },

      /** @private */
      iframeUrl_: {
        computed: 'computeIframeUrl_(doodle_)',
        type: String,
      },

      /** @private */
      duration_: {
        type: String,
        value: null,
      },

      /** @private */
      height_: {
        type: String,
        value: null,
      },

      /** @private */
      width_: {
        type: String,
        value: null,
      },

      /** @private */
      showShareDialog_: Boolean,
    };
  }

  constructor() {
    performance.mark('logo-creation-start');
    super();
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
    BrowserProxy.getInstance().handler.getDoodle().then(({doodle}) => {
      this.doodle_ = doodle;
      this.loaded_ = true;
      if (this.doodle_ && this.doodle_.content.interactiveDoodle) {
        this.width_ = `${this.doodle_.content.interactiveDoodle.width}px`;
        this.height_ = `${this.doodle_.content.interactiveDoodle.height}px`;
      }
    });
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(window, 'message', ({data}) => {
      if (data['cmd'] !== 'resizeDoodle') {
        return;
      }
      this.duration_ = assert(data.duration);
      this.height_ = assert(data.height);
      this.width_ = assert(data.width);
    });
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  /** @override */
  ready() {
    super.ready();
    performance.measure('logo-creation', 'logo-creation-start');
  }

  /**
   * @return {boolean}
   * @private
   */
  computeCanShowDoodle_() {
    return !!this.doodle_ &&
        /* We hide interactive doodles when offline. Otherwise, the iframe
           would show an ugly error page. */
        (!this.doodle_.content.interactiveDoodle || window.navigator.onLine);
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowLogo_() {
    return !this.doodleAllowed || (!!this.loaded_ && !this.canShowDoodle_);
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowDoodle_() {
    return !!this.doodleAllowed && this.canShowDoodle_;
  }

  /**
   * Called when a simple or animated doodle was clicked. Starts animation if
   * clicking preview image of animated doodle. Otherwise, opens
   * doodle-associated URL in new tab/window.
   * @private
   */
  onImageClick_() {
    if (!this.showAnimation_ && this.doodle_ &&
        this.doodle_.content.imageDoodle.animationUrl) {
      this.showAnimation_ = true;
      return;
    }
    BrowserProxy.getInstance().open(
        this.doodle_.content.imageDoodle.onClickUrl.url);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onImageKeydown_(e) {
    if ([' ', 'Enter'].includes(e.key)) {
      this.onImageClick_();
    }
  }

  /**
   * @return {string}
   * @private
   */
  computeImageUrl_() {
    return (this.doodle_ && this.doodle_.content.imageDoodle &&
            this.doodle_.content.imageDoodle.imageUrl) ?
        this.doodle_.content.imageDoodle.imageUrl.url :
        '';
  }

  /**
   * @return {string}
   * @private
   */
  computeAnimationUrl_() {
    return (this.doodle_ && this.doodle_.content.imageDoodle &&
            this.doodle_.content.imageDoodle.animationUrl) ?
        `image?${this.doodle_.content.imageDoodle.animationUrl.url}` :
        '';
  }

  /**
   * @return {string}
   * @private
   */
  computeIframeUrl_() {
    return (this.doodle_ && this.doodle_.content.interactiveDoodle) ?
        `iframe?${this.doodle_.content.interactiveDoodle.url.url}` :
        '';
  }

  /**
   * @param {string} value
   * @return {string}
   * @private
   */
  valueOrUnset_(value) {
    return value || 'unset';
  }

  /**
   * @param {skia.mojom.SkColor} skColor
   * @return {string}
   * @private
   */
  rgbaOrUnset_(skColor) {
    return skColor ? skColorToRgba(skColor) : 'unset';
  }

  /**
   * @param {!Event} e
   * @private
   */
  onShareButtonClick_(e) {
    e.stopPropagation();
    this.showShareDialog_ = true;
  }

  /** @private */
  onShareDialogClose_() {
    this.showShareDialog_ = false;
  }
}

customElements.define(LogoElement.is, LogoElement);
