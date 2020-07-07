// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_drawer/cr_drawer.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class KaleidoscopeSideNavContainerElement extends PolymerElement {
  static get is() {
    return 'kaleidoscope-side-nav-container';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  // Toggles the CrDrawerElement.
  toggle() {
    /** @type {!CrDrawerElement} */ (this.$.drawer).toggle();
  }
}

customElements.define(
    KaleidoscopeSideNavContainerElement.is,
    KaleidoscopeSideNavContainerElement);
