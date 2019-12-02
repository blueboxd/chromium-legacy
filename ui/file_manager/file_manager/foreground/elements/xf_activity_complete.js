// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Activity complete indicator custom element for use in PanelItem(s).
 */
class ActivityComplete extends HTMLElement {
  constructor() {
    super();

    const icon = document.createElement('iron-icon');
    icon.setAttribute('style', 'width: 36px; height: 36px;');
    this.attachShadow({mode: 'open'}).appendChild(icon);
  }

  /**
   * Gets the current status of the indication.
   * @return {string}
   * @public
   */
  get status() {
    return this.getAttribute('status');
  }

  /**
   * Sets the current status of the indication.
   * @param {string} status The status value: "success" || "failure".
   * @public
   */
  set status(status) {
    const ironIcon = this.shadowRoot.querySelector('iron-icon');
    ironIcon.setAttribute('icon', `files36:${status}`);
    this.setAttribute('status', status);
  }
}

customElements.define('xf-activity-complete', ActivityComplete);
