// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This element provides a layer between the settings-multidevice-subpage
 * element and the internet_page folder's network-summary-item. It is
 * responsible for loading initial tethering network data from the
 * chrome.networkingPrivate API as well as updating the data in real time. It
 * serves a role comparable to the internet_page's network-summary element.
 */

Polymer({
  is: 'settings-multidevice-tether-item',

  behaviors: [
    CrNetworkListenerBehavior,
    MultiDeviceFeatureBehavior,
  ],

  properties: {
    /**
     * Interface for networkingPrivate calls.
     * @private {!NetworkingPrivate}
     */
    networkingPrivate_: {
      type: Object,
      value: chrome.networkingPrivate,
    },

    /**
     * The device state for tethering.
     * @private {?CrOnc.DeviceStateProperties|undefined}
     */
    deviceState_: Object,

    /**
     * The network state for a potential tethering host phone. Note that there
     * is at most one because only one MultiDevice host phone is allowed on an
     * account at a given time.
     * @private {?CrOnc.NetworkStateProperties|undefined}
     */
    activeNetworkState_: Object,

    /**
     * Alias for allowing Polymer bindings to settings.routes.
     * @type {?SettingsRoutes}
     */
    routes: {
      type: Object,
      value: settings.routes,
    },

    /**
     * Whether to show technology badge on mobile network icon.
     * @private
     */
    showTechnologyBadge_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('showTechnologyBadge') &&
            loadTimeData.getBoolean('showTechnologyBadge');
      }
    },
  },

  /** @override */
  attached: function() {
    this.updateTetherDeviceState_();
    this.updateTetherNetworkState_();
  },

  /**
   * CrosNetworkConfigObserver impl
   * Note that any change to leading to a new active network will also trigger
   * onNetworkStateListChanged, triggering updateTetherNetworkState_ and
   * rendering this callback redundant. As a result, we return early if the
   * active network is not changed.
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   * @private
   */
  onActiveNetworksChanged: function(networks) {
    const id = this.activeNetworkState_.GUID;
    if (!networks.find(network => network.guid == id)) {
      return;
    }
    this.networkingPrivate_.getState(id, newNetworkState => {
      if (chrome.runtime.lastError) {
        const message = chrome.runtime.lastError.message;
        if (message != 'Error.NetworkUnavailable' &&
            message != 'Error.InvalidNetworkGuid') {
          console.error(
              'Unexpected networkingPrivate.getState error: ' + message +
              ' For: ' + id);
          return;
        }
      }
      this.activeNetworkState_ = newNetworkState;
    });
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.updateTetherNetworkState_();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    this.updateTetherDeviceState_();
  },

  /**
   * Retrieves device states (CrOnc.DeviceStateProperties) and sets
   * this.deviceState_ to the retrieved Instant Tethering state (or undefined if
   * there is none) in its callback. Note that the function
   * chrome.networkingPrivate.getDevicePolicy() retrieves at most one object per
   * network type (CrOnc.Type) so, in particular there will be at most one state
   * for Instant Tethering.
   * @private
   */
  updateTetherDeviceState_: function() {
    this.networkingPrivate_.getDeviceStates(deviceStates => {
      this.deviceState_ =
          deviceStates.find(
              deviceState => deviceState.Type == CrOnc.Type.TETHER) ||
          {Type: CrOnc.Type.TETHER, State: CrOnc.DeviceState.DISABLED};
    });
  },

  /**
   * Retrieves all Instant Tethering network states
   * (CrOnc.NetworkStateProperties). Note that there is at most one because
   * only one host is allowed on an account at a given time. Then it sets
   * this.activeNetworkState_ to that network if there is one or a dummy object
   * with an empty string for a GUID otherwise.
   * @private
   */
  updateTetherNetworkState_: function() {
    this.networkingPrivate_.getNetworks(
        {networkType: CrOnc.Type.TETHER}, networkStates => {
          this.activeNetworkState_ =
              networkStates[0] || {GUID: '', Type: CrOnc.Type.TETHER};
        });
  },

  /**
   * Returns an array containing the active network state if there is one
   * (note that if there is not GUID will be falsy).  Returns an empty array
   * otherwise.
   * @return {!Array<CrOnc.NetworkStateProperties>}
   * @private
   */
  getNetworkStateList_: function() {
    return this.activeNetworkState_.GUID ? [this.activeNetworkState_] : [];
  },

  /**
   * @return {!URLSearchParams}
   * @private
   */
  getTetherNetworkUrlSearchParams_: function() {
    return new URLSearchParams('type=' + CrOnc.Type.TETHER);
  },
});
