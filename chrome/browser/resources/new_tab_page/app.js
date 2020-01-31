// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './most_visited.js';
import './customize_dialog.js';
import './voice_search_overlay.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {skColorToRgb} from './utils.js';

class AppElement extends PolymerElement {
  static get is() {
    return 'ntp-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!newTabPage.mojom.Theme} */
      theme_: Object,

      /** @private */
      showCustomizeDialog_: Boolean,

      /** @private */
      showVoiceSearchOverlay_: Boolean,
    };
  }

  constructor() {
    super();
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    /** @private {?number} */
    this.setThemeListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener(theme => {
          this.theme_ = theme;
        });
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(assert(this.setThemeListenerId_));
  }

  /** @private */
  onVoiceSearchClick_() {
    this.showVoiceSearchOverlay_ = true;
  }

  /** @private */
  onCustomizeClick_() {
    this.showCustomizeDialog_ = true;
  }

  /** @private */
  onCustomizeDialogClose_() {
    this.showCustomizeDialog_ = false;
  }

  /** @private */
  onVoiceSearchOverlayClose_() {
    this.showVoiceSearchOverlay_ = false;
  }

  /**
   * @param {skia.mojom.SkColor} skColor
   * @return {string}
   * @private
   */
  rgbOrInherit_(skColor) {
    return skColor ? skColorToRgb(skColor) : 'inherit';
  }
}

customElements.define(AppElement.is, AppElement);
