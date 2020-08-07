// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-onboarding-page' component handles the Nearby Share
 * onboarding flow. It is embedded in chrome://os-settings, chrome://settings
 * and as a standalone dialog via chrome://nearby.
 */
Polymer({
  is: 'nearby-onboarding-page',

  properties: {
    /** @type {?nearby_share.NearbySettings} */
    settings: {
      type: Object,
    }
  },

  onNextTap_() {
    this.fire('change-page', {page: 'visibility'});
  },

  onCloseTap_() {
    this.fire('close');
  },

  onDeviceNameTap_() {
    window.open('chrome://os-settings/multidevice/nearbyshare?deviceName');
  },
});
