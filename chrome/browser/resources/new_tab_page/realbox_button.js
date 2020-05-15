// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// The RHS action button. Used on autocomplete matches as the remove button as
// well as on suggestion group headers as the toggle button.
class RealboxButtonElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  ready() {
    super.ready();

    this.addEventListener('mousedown', this.onMouseDown_.bind(this));
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @param {!Event} e
   */
  onMouseDown_(e) {
    e.preventDefault();  // Prevents default browser action (focus).
  }
}

customElements.define(RealboxButtonElement.is, RealboxButtonElement);
