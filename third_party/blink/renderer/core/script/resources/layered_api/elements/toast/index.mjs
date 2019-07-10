/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview This file defines the class for the Standard Toast LAPI
 * and the accompanying showToast() function.
 * EXPLAINER: https://github.com/jackbsteinberg/std-toast
 * TEST PATH: /chromium/src/third_party/blink/web_tests/external/wpt/std-toast/*
 * @package
 */

import * as reflection from '../internal/reflection.mjs';

const DEFAULT_DURATION = 3000;

function stylesheetFactory() {
  let stylesheet;
  return function generate() {
    if (!stylesheet) {
      stylesheet = new CSSStyleSheet();
      stylesheet.replaceSync(`
        :host {
          position: fixed;
          bottom: 0;
          right: 0;
          z-index: 1;
          background-color: #FFFFFF;
          color: #000000;
          font-size: 20px;
          border-color: #000000;
          border-style: solid;
          border-width: 2px;
          border-radius: 2.5px;
          padding: 10px;
          margin: 10px;
        }

        :host(:not([open])) {
          display: none;
        }
      `);
    }
    return stylesheet;
  };
}

const generateStylesheet = stylesheetFactory();

export class StdToastElement extends HTMLElement {
  static observedAttributes = ['open'];
  #shadow = this.attachShadow({mode: 'closed'});
  #timeoutID;
  #actionSlot;

  constructor(message) {
    super();

    this.#shadow.adoptedStyleSheets = [generateStylesheet()];

    this.#shadow.innerHTML = `<slot></slot>`;
    this.#actionSlot = document.createElement('slot');
    this.#actionSlot.setAttribute('name', 'action');
    this.#shadow.appendChild(this.#actionSlot);

    if (message !== undefined) {
      this.textContent = message;
    }
  }

  connectedCallback() {
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'status');
    }
    // TODO(jacksteinberg): use https://github.com/whatwg/html/pull/4658 when implemented
  }

  get action() {
    return this.#actionSlot.assignedNodes().length !== 0 ?
      this.#actionSlot.assignedNodes()[0] :
      null;
  }

  show({duration = DEFAULT_DURATION} = {}) {
    this.setAttribute('open', '');
    clearTimeout(this.#timeoutID);
    this.#timeoutID = setTimeout(() => {
      this.removeAttribute('open');
    }, duration);
  }

  hide() {
    this.removeAttribute('open');
  }

  toggle(force) {
    this.toggleAttribute('open', force);
  }

  attributeChangedCallback(name, oldValue, newValue) {
    switch (name) {
      case 'open':
        if (newValue !== null && oldValue === null) {
          this.dispatchEvent(new Event('show'));
        } else if (newValue === null) {
          this.dispatchEvent(new Event('hide'));
          clearTimeout(this.#timeoutID);
          this.#timeoutID = null;
        }
        break;
    }
  }
}

reflection.installBool(StdToastElement.prototype, 'open');

customElements.define('std-toast', StdToastElement);

delete StdToastElement.prototype.attributeChangedCallback;
delete StdToastElement.prototype.observedAttributes;
delete StdToastElement.prototype.connectedCallback;

export function showToast(message, options = {}) {
  const toast = new StdToastElement(message);

  const {action, ...showOptions} = options;
  if (action !== undefined) {
    const actionButton = document.createElement('button');

    // Unlike String(), this performs the desired JavaScript ToString operation.
    // https://gist.github.com/domenic/82adbe7edc4a33a70f42f255479cec39
    actionButton.textContent = `${action}`;

    actionButton.setAttribute('slot', 'action');
    toast.appendChild(actionButton);
  }

  document.body.append(toast);
  toast.show(showOptions);

  return toast;
}
