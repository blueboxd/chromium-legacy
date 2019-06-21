// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as face from './face_utils.mjs';
import * as reflection from './reflection.mjs';

// https://github.com/tkent-google/std-switch/issues/2
const STATE_ATTR = 'on';

// Private property symbols
// TODO(tkent): Use a private field.
const _internals = Symbol();

export class StdSwitchElement extends HTMLElement {
  static get formAssociated() {
    return true;
  }

  constructor() {
    super();
    this[_internals] = this.attachInternals();
  }
}

reflection.installBool(StdSwitchElement.prototype, STATE_ATTR);
reflection.installBool(
    StdSwitchElement.prototype, 'default' + STATE_ATTR,
    'default' + STATE_ATTR.charAt(0).toUpperCase() + STATE_ATTR.substring(1));
face.installPropertiesAndFunctions(StdSwitchElement.prototype, _internals);

// This is necessary for anyObject.toString.call(switchInstance).
Object.defineProperty(StdSwitchElement.prototype, Symbol.toStringTag, {
  configurable: true,
  enumerable: false,
  value: 'StdSwitchElement',
  writable: false
});

customElements.define('std-switch', StdSwitchElement);
