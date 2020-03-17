// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from './browser_proxy.js';

// A fake search box that moves focus and input to the omnibox when interacted
// with.
class FakeboxElement extends PolymerElement {
  static get is() {
    return 'ntp-fakebox';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      focused_: {
        reflectToAttribute: true,
        type: Boolean,
      },

      /** @private */
      hidden_: {
        reflectToAttribute: true,
        type: Boolean,
      },

      /** @private */
      dragged_: {
        reflectToAttribute: true,
        type: Boolean,
      },
    };
  }

  constructor() {
    super();
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    /** @private {?number} */
    this.setFakeboxFocusedListenerId_ = null;
    /** @private {?number} */
    this.setFakeboxVisibleListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setFakeboxFocusedListenerId_ =
        this.callbackRouter_.setFakeboxFocused.addListener(focused => {
          this.focused_ = focused;
          this.dragged_ = false;
        });
    this.setFakeboxVisibleListenerId_ =
        this.callbackRouter_.setFakeboxVisible.addListener(visible => {
          this.hidden_ = !visible;
        });
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.setFakeboxFocusedListenerId_));
    this.callbackRouter_.removeListener(
        assert(this.setFakeboxVisibleListenerId_));
  }

  /** @private */
  onMousedown_() {
    this.pageHandler_.focusOmnibox();
  }

  /**
   * @param {Event} e
   * @private
   */
  onPaste_(e) {
    e.preventDefault();
    const text = e.clipboardData.getData('text/plain');
    if (!text) {
      return;
    }
    this.pageHandler_.pasteIntoOmnibox(text);
  }

  /** @private */
  onDragenter_() {
    this.dragged_ = true;
  }

  /** @private */
  onDragleave_() {
    this.dragged_ = false;
  }

  /**
   * @param {Event} e
   * @private
   */
  onDrop_(e) {
    e.preventDefault();
    const text = e.dataTransfer.getData('text/plain');
    if (!text) {
      return;
    }
    this.pageHandler_.focusOmnibox();
    this.pageHandler_.pasteIntoOmnibox(text);
  }

  /** @private */
  onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }
}

customElements.define(FakeboxElement.is, FakeboxElement);
