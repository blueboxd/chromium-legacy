// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      /** @private */
      doodle_: Object,
    };
  }

  constructor() {
    super();
    BrowserProxy.getInstance().handler.getDoodle().then(({doodle}) => {
      this.doodle_ = doodle;
    });
  }
}

customElements.define(LogoElement.is, LogoElement);
