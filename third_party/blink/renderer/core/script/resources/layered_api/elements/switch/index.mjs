// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as face from './face_utils.mjs';
import * as reflection from '../internal/reflection.mjs';
import {SwitchTrack} from './track.mjs';
import * as style from './style.mjs';

const generateStyleSheet = style.styleSheetFactory();

// https://github.com/tkent-google/std-switch/issues/2
const STATE_ATTR = 'on';

// Private property symbols
// TODO(tkent): Use private fields.
const _internals = Symbol('an ElementInternals field');
const _track = Symbol('a track element field');
const _containerElement = Symbol('A container element field');

export class StdSwitchElement extends HTMLElement {
  // TODO(tkent): The following should be |static fooBar = value;|
  // after enabling babel-eslint.
  static get formAssociated() {
    return true;
  }
  static get observedAttributes() {
    return [STATE_ATTR];
  }

  #inUserAction = false;

  constructor() {
    super();
    if (new.target !== StdSwitchElement) {
      throw new TypeError('Illegal constructor: StdSwitchElement is not ' +
                          'extensible for now');
    }
    this[_internals] = this.attachInternals();
    this._initializeDOM();

    this.addEventListener('click', this._onClick);
    this.addEventListener('keypress', this._onKeyPress);
  }

  attributeChangedCallback(attrName, oldValue, newValue) {
    if (attrName === STATE_ATTR) {
      this[_track].value = newValue !== null;
      // TODO(tkent): We should not add aria-checked attribute.
      // https://github.com/WICG/aom/issues/127
      this.setAttribute('aria-checked', newValue !== null ? 'true' : 'false');
      if (!this.#inUserAction) {
        for (const element of this[_containerElement].querySelectorAll('*')) {
          style.unmarkTransition(element);
        }
      }
    }
  }

  connectedCallback() {
    // TODO(tkent): We should not add tabindex attribute.
    // https://github.com/w3c/webcomponents/issues/762
    if (!this.hasAttribute('tabindex')) {
      this.setAttribute('tabindex', '0');
    }

    // TODO(tkent): We should not add role attribute.
    // https://github.com/WICG/aom/issues/127
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'switch');
    }
  }

  // TODO(tkent): Make this private.
  _initializeDOM() {
    const factory = this.ownerDocument;
    const root = this.attachShadow({mode: 'closed'});
    this[_containerElement] = factory.createElement('span');
    this[_containerElement].id = 'container';
    // Shadow elements should be invisible for a11y technologies.
    this[_containerElement].setAttribute('aria-hidden', 'true');
    root.appendChild(this[_containerElement]);

    this[_track] = new SwitchTrack(factory);
    this[_containerElement].appendChild(this[_track].element);
    this[_track].value = this.on;

    const thumbElement =
        this[_containerElement].appendChild(factory.createElement('span'));
    thumbElement.id = 'thumb';
    thumbElement.part.add('thumb');

    root.adoptedStyleSheets = [generateStyleSheet()];
  }

  // TODO(tkent): Make this private.
  _onClick() {
    for (const element of this[_containerElement].querySelectorAll('*')) {
      style.markTransition(element);
    }
    this.#inUserAction = true;
    try {
      this.on = !this.on;
    } finally {
      this.#inUserAction = false;
    }
    this.dispatchEvent(new Event('input', {bubbles: true}));
    this.dispatchEvent(new Event('change', {bubbles: true}));
  }

  // TODO(tkent): Make this private.
  _onKeyPress(event) {
    if (event.code === 'Space') {
      // Do not scroll the page.
      event.preventDefault();
      this._onClick(event);
    }
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
  writable: false,
});

customElements.define('std-switch', StdSwitchElement);
delete StdSwitchElement.formAssociated;
delete StdSwitchElement.observedAttributes;
delete StdSwitchElement.prototype.attributeChangedCallback;
delete StdSwitchElement.prototype.connectedCallback;
