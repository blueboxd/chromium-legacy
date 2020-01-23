// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from './custom_element.js';
import {TabGroupVisualData} from './tabs_api_proxy.js';

export class TabGroupElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  /**
   * @param {!TabGroupVisualData} visualData
   */
  updateVisuals(visualData) {
    this.shadowRoot.querySelector('#title').innerText = visualData.title;
    this.style.setProperty('--tabstrip-tab-group-color-rgb', visualData.color);
    this.style.setProperty(
        '--tabstrip-tab-group-text-color-rgb', visualData.textColor);
  }
}

customElements.define('tabstrip-tab-group', TabGroupElement);
