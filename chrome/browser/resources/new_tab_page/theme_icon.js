// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Represents a theme. Displayed as a circle with each half colored based on
 * the custom CSS properties |--ntp-theme-icon-frame-color| and
 * |--ntp-theme-icon-active-tab-color|. Can be selected by setting the
 * |selected| attribute.
 */
class ThemeIconElement extends PolymerElement {
  static get is() {
    return 'ntp-theme-icon';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(ThemeIconElement.is, ThemeIconElement);
