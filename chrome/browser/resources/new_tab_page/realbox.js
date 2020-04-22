// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {skColorToRgba} from './utils.js';

// A real search box that behaves just like the Omnibox.
class RealboxElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!newTabPage.mojom.SearchBoxTheme} */
      theme: {
        type: Object,
        observer: 'onThemeChange_',
      },
    };
  }

  /** @private */
  onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }


  /** @private */
  onThemeChange_() {
    if (!loadTimeData.getBoolean('realboxMatchOmniboxTheme')) {
      return;
    }

    this.updateStyles({
      '--search-box-bg': skColorToRgba(assert(this.theme.bg)),
      '--search-box-placeholder': skColorToRgba(assert(this.theme.placeholder)),
      '--search-box-results-bg': skColorToRgba(assert(this.theme.resultsBg)),
      '--search-box-text': skColorToRgba(assert(this.theme.text)),
      '--search-box-icon': skColorToRgba(assert(this.theme.icon))
    });
  }
}

customElements.define(RealboxElement.is, RealboxElement);
