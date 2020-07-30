// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of NearbyShareSettings for testing.
 */
cr.define('nearby_share', function() {
  /**
   * Fake implementation of nearbyShare.mojom.NearbyShareSettingsInterface
   *
   * @implements {nearbyShare.mojom.NearbyShareSettingsInterface}
   */
  /* #export */ class FakeNearbyShareSettings {
    constructor() {
      /** @private {!boolean} */
      this.enabled_ = true;
      /** @private {!string} */
      this.deviceName_ = 'testDevice';
      /** @private {!nearbyShare.mojom.DataUsage} */
      this.dataUsage_ = nearbyShare.mojom.DataUsage.kOnline;
      /** @private {!nearbyShare.mojom.Visibility} */
      this.visibility_ = nearbyShare.mojom.Visibility.kAllContacts;
      /** @private {!Array<!string>} */
      this.allowedContacts_ = [];
      /** @private {!nearbyShare.mojom.NearbyShareSettingsObserverInterface} */
      this.observer_;
      /** @private {Object} */
      this.$ = {
        close() {},
      };
    }

    /**
     * @param { !nearbyShare.mojom.NearbyShareSettingsObserverInterface }
     *     observer
     */
    addSettingsObserver(observer) {
      // Just support a single observer for testing.
      this.observer_ = observer;
    }

    /**
     * @return {!Promise<{enabled: !boolean}>}
     */
    async getEnabled() {
      return {enabled: this.enabled_};
    }

    /**
     * @param { !boolean } enabled
     */
    setEnabled(enabled) {
      this.enabled_ = enabled;
      if (this.observer_) {
        this.observer_.onEnabledChanged(enabled);
      }
    }

    /**
     * @return {!Promise<{deviceName: !string}>}
     */
    async getDeviceName() {
      return {deviceName: this.deviceName_};
    }

    /**
     * @param { !string } deviceName
     */
    setDeviceName(deviceName) {
      this.deviceName_ = deviceName;
      if (this.observer_) {
        this.observer_.onDeviceNameChanged(deviceName);
      }
    }

    /**
     * @return {!Promise<{dataUsage: !nearbyShare.mojom.DataUsage}>}
     */
    async getDataUsage() {
      return {dataUsage: this.dataUsage_};
    }

    /**
     * @param { !nearbyShare.mojom.DataUsage } dataUsage
     */
    setDataUsage(dataUsage) {
      this.dataUsage_ = dataUsage;
      if (this.observer_) {
        this.observer_.onDataUsageChanged(dataUsage);
      }
    }

    /**
     * @return {!Promise<{visibility: !nearbyShare.mojom.Visibility}>}
     */
    async getVisibility() {
      return {visibility: this.visibility_};
    }

    /**
     * @param { !nearbyShare.mojom.Visibility } visibility
     */
    setVisibility(visibility) {
      this.visibility_ = visibility;
      if (this.observer_) {
        this.observer_.onVisibilityChanged(visibility);
      }
    }

    /**
     * @return {!Promise<{allowedContacts: !Array<!string>}>}
     */
    async getAllowedContacts() {
      return {allowedContacts: this.allowedContacts_};
    }

    /**
     * @param { !Array<!string> } allowedContacts
     */

    setAllowedContacts(allowedContacts) {
      this.allowedContacts_ = allowedContacts;
      if (this.observer_) {
        this.observer_.onAllowedContactsChanged(this.allowedContacts_);
      }
    }
  }
  // #cr_define_end
  return {FakeNearbyShareSettings: FakeNearbyShareSettings};
});
