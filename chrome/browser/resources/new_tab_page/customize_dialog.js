// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @typedef {{label:string, icon:string}} */
let Color;

/**
 * Dialog that lets the user customize the NTP such as the background color or
 * image.
 */
class CustomizeDialogElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<!Color>} */
      colors_: Array,
    };
  }

  constructor() {
    super();
    // Create a few rows of sample data.
    // TODO(crbug.com/1030459): Add real data source.
    this.colors_ = [];
    for (let i = 0; i < 20; i++) {
      this.colors_.push({
        label: 'Warm grey',
        icon:
            'data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHhtbG5zOnhsaW5rPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5L3hsaW5rIiB3aWR0aD0iNjQiIGhlaWdodD0iNjQiPjxkZWZzPjxwYXRoIGQ9Ik0zMiA2NEMxNC4zNCA2NCAwIDQ5LjY2IDAgMzJTMTQuMzQgMCAzMiAwczMyIDE0LjM0IDMyIDMyLTE0LjM0IDMyLTMyIDMyeiIgaWQ9ImEiLz48bGluZWFyR3JhZGllbnQgaWQ9ImIiIGdyYWRpZW50VW5pdHM9InVzZXJTcGFjZU9uVXNlIiB4MT0iMzIiIHkxPSIzMiIgeDI9IjMyLjA4IiB5Mj0iMzIiPjxzdG9wIG9mZnNldD0iMCUiIHN0b3AtY29sb3I9IiNGRkZGRkYiLz48c3RvcCBvZmZzZXQ9IjEwMCUiIHN0b3AtY29sb3I9IiNFM0RCRDciLz48L2xpbmVhckdyYWRpZW50PjxjbGlwUGF0aCBpZD0iYyI+PHVzZSB4bGluazpocmVmPSIjYSIvPjwvY2xpcFBhdGg+PC9kZWZzPjx1c2UgeGxpbms6aHJlZj0iI2EiIGZpbGw9InVybCgjYikiLz48ZyBjbGlwLXBhdGg9InVybCgjYykiPjx1c2UgeGxpbms6aHJlZj0iI2EiIGZpbGwtb3BhY2l0eT0iMCIgc3Ryb2tlPSIjRTNEQkQ3IiBzdHJva2Utd2lkdGg9IjIiLz48L2c+PC9zdmc+',
      });
      this.colors_.push(
          {
            label: 'Cool grey',
            icon:
                'data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHhtbG5zOnhsaW5rPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5L3hsaW5rIiB3aWR0aD0iNjQiIGhlaWdodD0iNjQiPjxkZWZzPjxwYXRoIGQ9Ik0zMiA2NEMxNC4zNCA2NCAwIDQ5LjY2IDAgMzJTMTQuMzQgMCAzMiAwczMyIDE0LjM0IDMyIDMyLTE0LjM0IDMyLTMyIDMyeiIgaWQ9ImEiLz48bGluZWFyR3JhZGllbnQgaWQ9ImIiIGdyYWRpZW50VW5pdHM9InVzZXJTcGFjZU9uVXNlIiB4MT0iMzIiIHkxPSIzMiIgeDI9IjMyLjA4IiB5Mj0iMzIiPjxzdG9wIG9mZnNldD0iMCUiIHN0b3AtY29sb3I9IiNEOURBREYiLz48c3RvcCBvZmZzZXQ9IjEwMCUiIHN0b3AtY29sb3I9IiNBN0FCQjciLz48L2xpbmVhckdyYWRpZW50PjxjbGlwUGF0aCBpZD0iYyI+PHVzZSB4bGluazpocmVmPSIjYSIvPjwvY2xpcFBhdGg+PC9kZWZzPjx1c2UgeGxpbms6aHJlZj0iI2EiIGZpbGw9InVybCgjYikiLz48ZyBjbGlwLXBhdGg9InVybCgjYykiPjx1c2UgeGxpbms6aHJlZj0iI2EiIGZpbGwtb3BhY2l0eT0iMCIgc3Ryb2tlPSIjQTdBQkI3IiBzdHJva2Utd2lkdGg9IjIiLz48L2c+PC9zdmc+',
          },
      );
    }
  }

  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  /** @private */
  onCancelClick_() {
    this.$.dialog.cancel();
  }

  /** @private */
  onDoneClick_() {
    this.$.dialog.close();
  }
}

customElements.define(CustomizeDialogElement.is, CustomizeDialogElement);
