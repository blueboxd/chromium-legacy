// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A panel to display a collection (or list) of PanelItem.
 */
class DisplayPanel extends HTMLElement {
  constructor() {
    super();
    const host = document.createElement('template');
    host.innerHTML = this.constructor.template_;
    this.attachShadow({mode: 'open'}).appendChild(host.content.cloneNode(true));

    /** @private {Element} */
    this.container_ = this.shadowRoot.querySelector('#container');

    /**
     * True if the panel is not visible.
     * @type {boolean}
     * @private
     */
    this.hidden_ = true;

    /**
     * True if the panel been collapsed into summary view.
     * @type {boolean}
     * @private
     */
    this.collapsed_ = false;

    /**
     * PanelItems that are being hosted in the UI.
     * @type {Array<PanelItem>}
     * @private
     */
    this.items_ = [];
  }

  /**
   * Static getter for the custom element template.
   * @private
   */
  static get template_() {
    return `<style>
              #container {
                  align-items: stretch;
                  background-color: #FFF;
                  box-shadow: -2px -1px rgba(60, 64, 67, 0.15),
                              -1px 2px rgba(60, 64, 67, 0.3),
                              2px 0px rgba(60, 64, 67, 0.15);
                  border-radius: 4px;
                  display: flex;
                  flex-direction: column;
                  max-width: min-content;
                  z-index: 100;
              }
            </style>
            <div id="container"></div>`;
  }

  /**
   * Add a panel entry element inside our display panel.
   * @return {PanelItem}
   * @public
   */
  addPanelItem() {
    const panel = document.createElement('xf-panel-item');
    panel.setAttribute('indicator', 'progress');
    this.container_.appendChild(panel);
    this.items_.push(panel);
    return panel;
  }

  /**
   * Remove a panel from this display panel.
   * @param {PanelItem} item The PanelItem to remove.
   * @public
   */
  removePanelItem(item) {
    const pos = this.items_.indexOf(item);
    if (pos === -1) {
      return;
    }
    item.remove();
    this.items_.splice(pos, 1);
  }
}

window.customElements.define('xf-display-panel', DisplayPanel);
