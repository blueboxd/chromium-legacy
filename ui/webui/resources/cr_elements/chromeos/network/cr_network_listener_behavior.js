// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for observing CrosNetworkConfigObserver
 * events.
 */

/** @polymerBehavior */
const CrNetworkListenerBehavior = {
  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigObserver} */
  observer_: null,

  /** @override */
  attached: function() {
    this.observer_ =
        new chromeos.networkConfig.mojom.CrosNetworkConfigObserver(this);
    network_config.MojoInterfaceProviderImpl.getInstance()
        .getMojoServiceProxy()
        .addObserver(this.observer_.$.createProxy());
  },

  // CrosNetworkConfigObserver methods. Override these in the implementation.

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     activeNetworks
   */
  onActiveNetworksChanged: function(activeNetworks) {},

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {},

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {},
};
