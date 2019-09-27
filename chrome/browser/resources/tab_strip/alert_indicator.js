// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from './custom_element.js';

export class AlertIndicatorElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  /** @override */
  remove() {
    this.toggleAttribute('fade-out', true);
    this.addEventListener('animationend', () => super.remove(), {once: true});
  }
}

customElements.define('tabstrip-alert-indicator', AlertIndicatorElement);
