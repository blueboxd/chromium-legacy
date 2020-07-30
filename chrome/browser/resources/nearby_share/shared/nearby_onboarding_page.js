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

  /** @private { ?nearbyShare.mojom.NearbyShareSettingsInterface } */
  nearbyShareSettings_: null,
  /** @private  { ?nearbyShare.mojom.NearbyShareSettingsObserverInterface } */
  observer_: null,
  /** @private { ?nearbyShare.mojom.NearbyShareSettingsObserverReceiver } */
  observerReceiver_: null,

  attached() {
    this.nearbyShareSettings_ = nearby_share.getNearbyShareSettings();

    /**
     * @implements { nearbyShare.mojom.NearbyShareSettingsObserverInterface }
     */
    class Observer {
      /**
       * @param { !boolean } enabled
       * @override
       */
      onEnabledChanged(enabled) {
        console.log('onEnabledUpdated():', enabled);
      }

      /**
       * @param { !string } deviceName
       * @override
       */
      onDeviceNameChanged(deviceName) {
        console.log('onDeviceNameUpdated():', deviceName);
      }

      /**
       * @param { !nearbyShare.mojom.DataUsage } dataUsage
       * @override
       */
      onDataUsageChanged(dataUsage) {
        console.log('onDataUsageUpdated():', dataUsage);
      }

      /**
       * @param { !nearbyShare.mojom.Visibility } visibility
       * @override
       */
      onVisibilityChanged(visibility) {
        console.log('onVisibilityUpdated():', visibility);
      }

      /**
       * @param { !Array<!string> } allowedContacts
       * @override
       */
      onAllowedContactsChanged(allowedContacts) {
        console.log('onAllowedContactsChanged():', allowedContacts);
      }
    }

    this.observer_ = new Observer();
    this.observerReceiver_ =
        nearby_share.observeNearbyShareSettings(this.observer_);
  },

  detached() {
    if (this.observerReceiver_) {
      this.observerReceiver_.$.close();
    }
    if (this.nearbyShareSettings_) {
      /** @type { nearbyShare.mojom.NearbyShareSettingsRemote } */
      (this.nearbyShareSettings_).$.close();
    }
  }
});
