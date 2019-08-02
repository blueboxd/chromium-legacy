// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from './custom_element.js';

class Tab extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  connectedCallback() {
    this.setAttribute('tabindex', 0);
  }
}

customElements.define('tabstrip-tab', Tab);
