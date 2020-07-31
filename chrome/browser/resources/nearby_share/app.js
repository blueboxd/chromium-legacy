// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-share' component is the entry point for the Nearby
 * Share flow. It is used as a standalone dialog via chrome://nearby and as part
 * of the ChromeOS share sheet.
 */

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.m.js';
import './shared/nearby_onboarding_page.m.js';
import './nearby_confirmation_page.js';
import './nearby_discovery_page.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
const Page = {
  CONFIRMATION: 'confirmation',
  DISCOVERY: 'discovery',
  ONBOARDING: 'onboarding',
};

Polymer({
  is: 'nearby-share-app',

  _template: html`{__html_template__}`,

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    Page: {
      type: Object,
      value: Page,
    },

    /**
     * Set by the nearby-discovery-page component when switching to the
     * nearby-confirmation-page.
     * @type {?nearbyShare.mojom.ConfirmationManagerInterface}
     * @private
     */
    confirmationManager_: {
      type: Object,
      value: null,
    },

    /**
     * Set by the nearby-discovery-page component when switching to the
     * nearby-confirmation-page.
     * @type {?String}
     * @private
     */
    confirmationToken_: {
      type: String,
      value: null,
    },

    /**
     * The currently selected share target set by the nearby-discovery-page
     * component when the user selects a device.
     * @type {?nearbyShare.mojom.ShareTarget}
     * @private
     */
    selectedShareTarget_: {
      type: Object,
      value: null,
    },
  },

  listeners: {'change-page': 'onChangePage_'},

  /**
   * Handler for the change-page event.
   * @param {!CustomEvent<!{page: Page}>} event
   * @private
   */
  onChangePage_(event) {
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(event.detail.page);
  },
});
