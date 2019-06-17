// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A button used inside PanelItem with varying display characteristics.
 */
class PanelButton extends HTMLElement {
  constructor() {
    PanelButton.createElement_.call(super());
  }

  /**
   * Creates an instance of Panelitem, attaching the template clone.
   * @private
   * @return {PanelButton} Custom element instance.
   */
  static createElement_() {
    const template = document.createElement('template');
    template.innerHTML = PanelButton.html_();
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Get the custom element template string.
   * TODO(crbug.com/947388) Replace 'button' to WebUI's 'cr-button'.
   * @private
   * @return {string}
   */
  static html_() {
    return `<style>
              button {
                background: none;
                border: none;
                height: 36px;
                width: 36px;
                position: relative;
              }

              :host([data-category='pause']) {
                background: url(../images/files/ui/pause.svg) no-repeat center;
              }

              :host([data-category='cancel']) {
                background: url(../images/files/ui/cancel.svg) no-repeat center;
              }

              :host([data-category='expand']),
              :host([data-category='collapse']) {
                background: url(../images/files/ui/expand.svg) no-repeat center;
              }

              @keyframes setcollapse {
                from {
                  transform: rotate(0deg);
                }
                to {
                  transform: rotate(180deg);
                }
              }

              @keyframes setexpand {
                from {
                  transform: rotate(-180deg);
                }
                to {
                  transform: rotate(0deg);
                }
              }

              :host([data-category='expand']) {
                  animation: setexpand 1s forwards;
              }

              :host([data-category='collapse']) {
                  animation: setcollapse 1s forwards;
              }
            </style>
            <button>
                <paper-ripple class="circle" center></paper-ripple>
            </button>`;
  }
}

window.customElements.define('xf-button', PanelButton);
