// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from './custom_element.js';
import {TabGroupVisualData} from './tabs_api_proxy.js';

export class TabGroupElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  /** @return {!HTMLElement} */
  getDragImage() {
    return /** @type {!HTMLElement} */ (this.$('#dragImage'));
  }

  /** @param {boolean} enabled */
  setDragging(enabled) {
    // Since the draggable target is the #chip, if the #chip moves and is no
    // longer under the pointer while the dragstart event is happening, the drag
    // will get canceled. This is unfortunately the behavior of the native drag
    // and drop API. The workaround is to have two different attributes: one
    // to get the drag image and start the drag event while keeping #chip in
    // place, and another to update the placeholder to take the place of where
    // the #chip would be.
    this.toggleAttribute('getting-drag-image_', enabled);
    requestAnimationFrame(() => {
      this.toggleAttribute('dragging', enabled);
    });
  }

  /**
   * @param {!TabGroupVisualData} visualData
   */
  updateVisuals(visualData) {
    this.$('#title').innerText = visualData.title;
    this.style.setProperty('--tabstrip-tab-group-color-rgb', visualData.color);
    this.style.setProperty(
        '--tabstrip-tab-group-text-color-rgb', visualData.textColor);
  }
}

customElements.define('tabstrip-tab-group', TabGroupElement);
