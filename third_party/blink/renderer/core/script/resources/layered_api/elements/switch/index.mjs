// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as face from './face_utils.mjs';
import * as reflection from './reflection.mjs';
import { SwitchTrack } from './track.mjs';

// https://github.com/tkent-google/std-switch/issues/2
const STATE_ATTR = 'on';

// Private property symbols
// TODO(tkent): Use private fields.
const _internals = Symbol();
const _track = Symbol();
const _rippleElement = Symbol();

export class StdSwitchElement extends HTMLElement {
  // TODO(tkent): The following should be |static fooBar = value;|
  // after enabling babel-eslint.
  static get formAssociated() {
    return true;
  }
  static get observedAttributes() {
    return [STATE_ATTR];
  }

  constructor() {
    super();
    if (new.target !== StdSwitchElement) {
      throw new TypeError('Illegal constructor: StdSwitchElement is not ' +
                          'extensible for now');
    }
    this[_internals] = this.attachInternals();
    this._initializeDOM();
  }

  attributeChangedCallback(attrName, oldValue, newValue) {
    if (attrName == STATE_ATTR) {
      this[_track].value = newValue !== null;
    }
  }

  // TODO(tkent): Make this private.
  _initializeDOM() {
    let factory = this.ownerDocument;
    let root = this.attachShadow({mode: 'closed'});
    let container = factory.createElement('span');
    container.classList.add('container');
    root.appendChild(container);

    this[_track] = new SwitchTrack(factory);
    container.appendChild(this[_track].element);
    this[_track].value = this.on;

    let thumbElement = container.appendChild(factory.createElement('span'));
    thumbElement.classList.add('thumb');

    this[_rippleElement] = thumbElement.appendChild(factory.createElement('span'));
    this[_rippleElement].classList.add('ripple');

    // TODO(tkent): Add stylesheets for std-switch too.
    root.adoptedStyleSheets = [this[_track].styleSheet];
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
delete StdSwitchElement.formAssociated;
delete StdSwitchElement.observedAttributes;
delete StdSwitchElement.prototype.attributeChangedCallback;
