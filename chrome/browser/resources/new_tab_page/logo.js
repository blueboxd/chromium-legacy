// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './untrusted_iframe.js';
import './doodle_share_dialog.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {$$, skColorToRgba} from './utils.js';

/** @type {number} */
const SHARE_BUTTON_SIZE_PX = 26;

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

      /**
       * If true displays the dark mode doodle if possible.
       * @type {boolean}
       */
      dark: {
        observer: 'onDarkChange_',
        type: Boolean,
      },

      /**
       * The NTP's background color. If null or undefined the NTP does not have
       * a single background color, e.g. when a background image is set.
       * @type {skia.mojom.SkColor}
       */
      backgroundColor: Object,

      /** @private */
      loaded_: Boolean,

      /** @private {newTabPage.mojom.Doodle} */
      doodle_: {
        observer: 'onDoodleChange_',
        type: Object,
      },

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
      doodleBoxed_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeDoodleBoxed_(backgroundColor, doodle_)',
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
        observer: 'onDurationHeightWidthChange_',
        type: String,
      },

      /** @private */
      height_: {
        observer: 'onDurationHeightWidthChange_',
        type: String,
      },

      /** @private */
      width_: {
        observer: 'onDurationHeightWidthChange_',
        type: String,
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
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    this.pageHandler_.getDoodle().then(({doodle}) => {
      this.doodle_ = doodle;
      this.loaded_ = true;
      if (this.doodle_ && this.doodle_.content.interactiveDoodle) {
        this.width_ = `${this.doodle_.content.interactiveDoodle.width}px`;
        this.height_ = `${this.doodle_.content.interactiveDoodle.height}px`;
      }
    });
    /** @private {?string} */
    this.imageClickParams_;
    /** @private {url.mojom.Url} */
    this.interactionLogUrl_;
    /** @private {?string} */
    this.shareId_;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(window, 'message', ({data}) => {
      if (data['cmd'] === 'resizeDoodle') {
        this.duration_ = assert(data.duration);
        this.height_ = assert(data.height);
        this.width_ = assert(data.width);
      } else if (data['cmd'] === 'sendMode') {
        this.sendMode_();
      }
    });
    // Make sure the doodle gets the mode in case it has already requested it.
    this.sendMode_();
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

  /** @private */
  onDoodleChange_() {
    const imageDoodle = this.doodle_ && this.doodle_.content.imageDoodle;
    this.updateStyles({
      '--ntp-logo-share-button-background-color':
          imageDoodle && skColorToRgba(imageDoodle.shareButton.backgroundColor),
      '--ntp-logo-share-button-height':
          imageDoodle && `${SHARE_BUTTON_SIZE_PX / imageDoodle.height * 100}%`,
      '--ntp-logo-share-button-width':
          imageDoodle && `${SHARE_BUTTON_SIZE_PX / imageDoodle.width * 100}%`,
      '--ntp-logo-share-button-x': imageDoodle &&
          `${imageDoodle.shareButton.x / imageDoodle.width * 100}%`,
      '--ntp-logo-share-button-y': imageDoodle &&
          `${imageDoodle.shareButton.y / imageDoodle.height * 100}%`,
      '--ntp-logo-box-color':
          imageDoodle && skColorToRgba(imageDoodle.backgroundColor),
    });
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
   * @return {boolean}
   * @private
   */
  computeDoodleBoxed_() {
    return !this.backgroundColor ||
        !!this.doodle_ && !!this.doodle_.content.imageDoodle &&
        this.doodle_.content.imageDoodle.backgroundColor.value !==
            this.backgroundColor.value;
  }

  /**
   * Called when a simple or animated doodle was clicked. Starts animation if
   * clicking preview image of animated doodle. Otherwise, opens
   * doodle-associated URL in new tab/window.
   * @private
   */
  onImageClick_() {
    if (this.isCtaImageShown_()) {
      this.showAnimation_ = true;
      this.pageHandler_.onDoodleImageClicked(
          newTabPage.mojom.DoodleImageType.CTA, this.interactionLogUrl_);

      // TODO(tiborg): This is technically not correct since we don't know if
      // the animation has loaded yet. However, since the animation is loaded
      // inside an iframe retrieving the proper load signal is not trivial. In
      // practice this should be good enough but we could improve that in the
      // future.
      this.logImageRendered_(
          newTabPage.mojom.DoodleImageType.ANIMATION,
          /** @type {!url.mojom.Url} */
          (this.doodle_.content.imageDoodle.animationImpressionLogUrl));

      return;
    }
    this.pageHandler_.onDoodleImageClicked(
        this.showAnimation_ ? newTabPage.mojom.DoodleImageType.ANIMATION :
                              newTabPage.mojom.DoodleImageType.STATIC,
        null);
    const onClickUrl = new URL(this.doodle_.content.imageDoodle.onClickUrl.url);
    if (this.imageClickParams_) {
      for (const param of new URLSearchParams(this.imageClickParams_)) {
        onClickUrl.searchParams.append(param[0], param[1]);
      }
    }
    BrowserProxy.getInstance().open(onClickUrl.toString());
  }

  /** @private */
  onImageLoad_() {
    this.logImageRendered_(
        this.isCtaImageShown_() ? newTabPage.mojom.DoodleImageType.CTA :
                                  newTabPage.mojom.DoodleImageType.STATIC,
        this.doodle_.content.imageDoodle.imageImpressionLogUrl);
  }

  /**
   * @param {newTabPage.mojom.DoodleImageType} type
   * @param {!url.mojom.Url} logUrl
   * @private
   */
  async logImageRendered_(type, logUrl) {
    const {imageClickParams, interactionLogUrl, shareId} =
        await this.pageHandler_.onDoodleImageRendered(
            type, BrowserProxy.getInstance().now(), logUrl);
    this.imageClickParams_ = imageClickParams;
    this.interactionLogUrl_ = interactionLogUrl;
    this.shareId_ = shareId;
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
   * @param {!CustomEvent} e
   * @private
   */
  onShare_(e) {
    const doodleId = new URL(this.doodle_.content.imageDoodle.onClickUrl.url)
                         .searchParams.get('ct');
    if (!doodleId) {
      return;
    }
    this.pageHandler_.onDoodleShared(e.detail, doodleId, this.shareId_);
  }

  /**
   * @returns {boolean}
   * @private
   */
  isCtaImageShown_() {
    return !this.showAnimation_ && !!this.doodle_ &&
        !!this.doodle_.content.imageDoodle.animationUrl;
  }

  /**
   * Sends a postMessage to the interactive doodle whether the  current theme is
   * dark or light. Won't do anything if we don't have an interactive doodle or
   * we haven't been told yet whether the current theme is dark or light.
   * @private
   */
  sendMode_() {
    const iframe = $$(this, '#iframe');
    if (!loadTimeData.getBoolean('themeModeDoodlesEnabled') ||
        this.dark === undefined || !iframe) {
      return;
    }
    BrowserProxy.getInstance().postMessage(
        iframe,
        {
          cmd: 'changeMode',
          dark: this.dark,
        },
        new URL(iframe.src).origin,
    );
  }

  /** @private */
  onDarkChange_() {
    this.sendMode_();
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
    if (this.doodle_ && this.doodle_.content.interactiveDoodle) {
      const url = new URL(this.doodle_.content.interactiveDoodle.url.url);
      if (loadTimeData.getBoolean('themeModeDoodlesEnabled')) {
        url.searchParams.append('theme_messages', '0');
      }
      return url.href;
    } else {
      return '';
    }
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

  /** @private */
  onDurationHeightWidthChange_() {
    this.updateStyles({
      '--duration': this.duration_,
      '--height': this.height_,
      '--width': this.width_,
    });
  }
}

customElements.define(LogoElement.is, LogoElement);
