// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from './custom_element.js';

export class TabGroupElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }
}

customElements.define('tabstrip-tab-group', TabGroupElement);
