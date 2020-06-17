// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying NetworkHealth properties.
 * TODO(1076178): Replace raw JSON output with something user friendly.
 */
Polymer({
  is: 'network-health-summary',

  properties: {},

  /** @private {?chromeos.networkHealth.mojom.NetworkHealthServiceRemote} */
  networkHealth_: null,

  /** @override */
  created() {
    this.networkHealth_ =
        chromeos.networkHealth.mojom.NetworkHealthService.getRemote();
  },

  /** @override */
  attached() {
    this.requestNetworkHealth_();
  },

  /** @private */
  requestNetworkHealth_() {
    this.networkHealth_.getHealthSnapshot().then(result => {
      this.$$('#network-health').textContent =
          JSON.stringify(result, null, '\t');
    });
  },

});
