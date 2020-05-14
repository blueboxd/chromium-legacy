// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {decodeString16} from './utils.js';

// Displays an autocomplete match similar to those in the Omnibox.
class RealboxMatchElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-match';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * @type {!search.mojom.AutocompleteMatch}
       */
      match: {
        type: Object,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Used to separate the contents from the description.
       * @type {string}
       * @private
       */
      separatorText_: {
        type: String,
        value: () => loadTimeData.getString('realboxSeparator'),
      }
    };
  }

  //============================================================================
  // Public methods
  //============================================================================

  /**
   * Focuses the <a> child element.
   * @override
   */
  focus() {
    this.$.link.focus();
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * Converts a mojoBase.mojom.String16 to a JavaScript String.
   * @param {?mojoBase.mojom.String16} str
   * @return {string}
   * @private
   */
  decodeString16_(str) {
    return decodeString16(str);
  }
}

customElements.define(RealboxMatchElement.is, RealboxMatchElement);
