// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from './browser_proxy.js';

class UntrustedIframeElement extends PolymerElement {
  static get is() {
    return 'ntp-untrusted-iframe';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {string} */
      path: {
        reflectToAttribute: true,
        type: String,
      },

      /** @private */
      src_: {
        type: String,
        computed: 'computeSrc_(path)',
      },
    };
  }

  /**
   * Sends message to iframe.
   * @param {*} message
   */
  postMessage(message) {
    this.$.iframe.contentWindow.postMessage(
        message, 'chrome-untrusted://new-tab-page');
  }

  /** @private */
  computeSrc_() {
    return BrowserProxy.getInstance().createUntrustedIframeSrc(this.path);
  }
}

customElements.define(UntrustedIframeElement.is, UntrustedIframeElement);
