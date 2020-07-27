// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-discovery-page' component shows the discovery UI of
 * the Nearby Share flow. It shows a list of devices to select from.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './nearby_device.js';
import './nearby_preview.js';
import './nearby_share.mojom-lite.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getDiscoveryManager} from './discovery_manager.js';

Polymer({
  is: 'nearby-discovery-page',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * ConfirmationManager interface for the currently selected share target.
     * @type {?nearbyShare.mojom.ConfirmationManagerInterface}
     */
    confirmationManager: {
      notify: true,
      type: Object,
      value: null,
    },

    /**
     * Token to show to the user to confirm the selected share target.
     * @type {?string}
     */
    confirmationToken: {
      notify: true,
      type: String,
      value: null,
    },
  },

  /** @private */
  onNextTap_() {
    // TODO(knollr): Use the selected device after discovering it.
    const shareTargetId = {high: 0, low: 17};

    getDiscoveryManager().selectShareTarget(shareTargetId).then(response => {
      const {result, token, confirmationManager} = response;
      if (result !== nearbyShare.mojom.SelectShareTargetResult.kOk) {
        // TODO(knollr): Show error.
        return;
      }

      if (confirmationManager) {
        this.confirmationManager = confirmationManager;
        this.confirmationToken = token;
        this.fire('change-page', {page: 'confirmation'});
      } else {
        // TODO(knollr): Close dialog as send is now in progress.
      }
    });
  },
});
