// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A button used inside PanelItem with varying display characteristics.
 */
class PanelButton extends HTMLElement {
  constructor() {
    super();
    this.createElement_();
  }

  /**
   * Creates a PanelButton.
   * @private
   */
  createElement_() {
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
              cr-icon-button, cr-button {
                margin-inline-start: 0px;
              }

              :host([data-category='pause']) {
                background: url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0naHR\
                  0cDovL3d3dy53My5vcmcvMjAwMC9zdmcnIHdpZHRoPSczNnB4JyBoZWlnaHQ\
                  9JzM2cHgnIHZpZXdCb3g9JzAgMCAzNiAzNic+CiAgICA8ZyBzdHJva2U9JyM\
                  1RjYzNjgnIHN0cm9rZS13aWR0aD0nMyc+CiAgICAgICAgPGxpbmUgeDE9JzE\
                  1JyB5MT0nMTInIHgyPScxNScgeTI9JzI0Jy8+CiAgICAgICAgPGxpbmUgeDE\
                  9JzIxJyB5MT0nMTInIHgyPScyMScgeTI9JzI0Jy8+CiAgICA8L2c+Cjwvc3Z\
                  nPg==') no-repeat center;
              }

              :host([data-category='cancel']) {
                background: url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0naHR\
                  0cDovL3d3dy53My5vcmcvMjAwMC9zdmcnIHdpZHRoPSczNnB4JyBoZWlnaHQ\
                  9JzM2cHgnIHZpZXdCb3g9JzAgMCAzNiAzNic+CiAgICA8ZyBzdHJva2U9JyM\
                  1RjYzNjgnIHN0cm9rZS13aWR0aD0nMic+CiAgICAgICAgPGxpbmUgeDE9JzE\
                  yJyB5MT0nMTInIHgyPScyNCcgeTI9JzI0Jy8+CiAgICAgICAgPGxpbmUgeDE\
                  9JzI0JyB5MT0nMTInIHgyPScxMicgeTI9JzI0Jy8+CiAgICA8L2c+Cjwvc3Z\
                  nPg==') no-repeat center;
              }

              :host([data-category='expand']),
              :host([data-category='collapse']) {
                background: url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0naHR\
                  0cDovL3d3dy53My5vcmcvMjAwMC9zdmcnIHdpZHRoPSczNnB4JyBoZWlnaHQ\
                  9JzM2cHgnIHZpZXdCb3g9JzAgMCAzNiAzNic+CiAgICA8ZyBzdHJva2U9JyM\
                  1RjYzNjgnIHN0cm9rZS13aWR0aD0nMic+CiAgICAgICAgPHBhdGggZmlsbD0\
                  ibm9uZSIgZD0nTTEyLDIxbDYsLTYgNiw2Jy8+CiAgICA8L2c+Cjwvc3ZnPg=='
                  ) no-repeat center;
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
                  animation: setexpand 200ms forwards;
              }

              :host([data-category='collapse']) {
                  animation: setcollapse 200ms forwards;
              }

              :host {
                position: relative;
              }

              :host(:not([data-category='dismiss'])) {
                width: 36px;
              }

              :host([data-category='dismiss']) #icon {
                display: none;
              }

              :host(:not([data-category='dismiss'])) #dismiss {
                display: none;
              }
            </style>
            <cr-button id='dismiss'>$i18n{DRIVE_WELCOME_DISMISS}</cr-button>
            <cr-icon-button id='icon'></cr-icon-button>`;
  }
}

window.customElements.define('xf-button', PanelButton);
