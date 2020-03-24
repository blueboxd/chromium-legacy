// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './untrusted_iframe.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from './browser_proxy.js';

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

      /** @private */
      doodle_: Object,

      /** @private */
      mode_: {
        computed: 'computeMode_(doodleAllowed, loaded_, doodle_)',
        type: Boolean,
      },

      /** @private */
      imageUrl_: {
        computed: 'computeImageUrl_(doodle_)',
        type: String,
      },

      /** @private */
      iframeUrl_: {
        computed: 'computeIframeUrl_(doodle_)',
        type: String,
      },
    };
  }

  constructor() {
    super();
    BrowserProxy.getInstance().handler.getDoodle().then(({doodle}) => {
      this.doodle_ = doodle;
      this.loaded_ = true;
    });
  }

  /**
   * @return {string}
   * @private
   */
  computeMode_() {
    if (this.doodleAllowed) {
      if (!this.loaded_) {
        return 'none';
      }
      if (this.doodle_ &&
          /* We hide interactive doodles when offline. Otherwise, the iframe
             would show an ugly error page. */
          (!this.doodle_.content.url || window.navigator.onLine)) {
        return 'doodle';
      }
    }
    return 'logo';
  }

  /**
   * @return {string}
   * @private
   */
  computeImageUrl_() {
    return (this.doodle_ && this.doodle_.content.image) ?
        this.doodle_.content.image :
        '';
  }

  /**
   * @return {string}
   * @private
   */
  computeIframeUrl_() {
    return (this.doodle_ && this.doodle_.content.url) ?
        `iframe?${this.doodle_.content.url.url}` :
        '';
  }
}

customElements.define(LogoElement.is, LogoElement);
