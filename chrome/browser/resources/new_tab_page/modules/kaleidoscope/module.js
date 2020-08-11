// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../grid.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ModuleDescriptor} from '../module_descriptor.js';

/**
 * @fileoverview The Kaleidoscope module which will serve Kaleidoscope
 * recommendations to the user through the NTP.
 */

class KaleidoscopeModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-kaleidoscope-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      tiles: {
        type: Array,
        value: () => ([
          {label: 'item1', value: 'foo'},
          {label: 'item2', value: 'bar'},
          {label: 'item3', value: 'baz'},
          {label: 'item4', value: 'boo'},
        ]),
      }
    };
  }
}

customElements.define(KaleidoscopeModuleElement.is, KaleidoscopeModuleElement);

/** @type {!ModuleDescriptor} */
export const kaleidoscopeDescriptor = {
  id: 'kaleidoscope',
  create: () => Promise.resolve(new KaleidoscopeModuleElement()),
};
