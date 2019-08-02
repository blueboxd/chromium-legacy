// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab.js';

import {CustomElement} from './custom_element.js';

class TabList extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }
}

customElements.define('tabstrip-tab-list', TabList);
