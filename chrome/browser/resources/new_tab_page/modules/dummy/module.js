// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../grid.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ModuleDescriptor} from '../module_descriptor.js';

/**
 * @fileoverview A dummy module, which serves as an example and a helper to
 * build out the NTP module framework.
 */

class DummyModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-dummy-module';
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
        ]),
      }
    };
  }
}

customElements.define(DummyModuleElement.is, DummyModuleElement);

/** @type {!ModuleDescriptor} */
export const dummyDescriptor = {
  id: 'dummy',
  create: () => Promise.resolve(new DummyModuleElement()),
};
