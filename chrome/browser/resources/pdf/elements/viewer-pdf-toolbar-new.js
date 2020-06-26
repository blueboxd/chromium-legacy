// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class ViewerPdfToolbarNewElement extends PolymerElement {
  static get is() {
    return 'viewer-pdf-toolbar-new';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(
    ViewerPdfToolbarNewElement.is, ViewerPdfToolbarNewElement);
