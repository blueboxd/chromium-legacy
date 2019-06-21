// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping cr-network-list including the
 * networkingPrivate calls to populate it.
 */

Polymer({
  is: 'cr-network-select',

  behaviors: [CrNetworkListenerBehavior],

  properties: {
    /**
     * Show all buttons in list items.
     */
    showButtons: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * The list of custom items to display after the list of networks.
     * See CrNetworkList for details.
     * @type {!Array<CrNetworkList.CustomItemState>}
     */
    customItems: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** Whether to show technology badges on mobile network icons. */
    showTechnologyBadge: {type: Boolean, value: true},

    /**
     * Whether to show a progress indicator at the top of the network list while
     * a scan (e.g., for nearby Wi-Fi networks) is in progress.
     */
    showScanProgress: {type: Boolean, value: false},

    /**
     * List of all network state data for all visible networks.
     * @private {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Whether a network scan is currently in progress.
     * @private
     */
    isScanOngoing_: {type: Boolean, value: false},

    /**
     * Cached Cellular Device state or undefined if there is no Cellular device.
     * @private {!CrOnc.DeviceStateProperties|undefined} deviceState
     */
    cellularDeviceState_: Object,
  },

  /** @type {!CrOnc.NetworkStateProperties|undefined} */
  defaultNetworkState_: undefined,


  /** @private {number|null} */
  scanIntervalId_: null,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigProxy} */
  networkConfigProxy_: null,

  /** @override */
  created: function() {
    this.networkConfigProxy_ =
        network_config.MojoInterfaceProviderImpl.getInstance()
            .getMojoServiceProxy();
  },

  /** @override */
  attached: function() {
    this.refreshNetworks();

    const INTERVAL_MS = 10 * 1000;
    const kAll = chromeos.networkConfig.mojom.NetworkType.kAll;
    this.networkConfigProxy_.requestNetworkScan(kAll);
    this.scanIntervalId_ = window.setInterval(function() {
      this.networkConfigProxy_.requestNetworkScan(kAll);
    }.bind(this), INTERVAL_MS);
  },

  /** @override */
  detached: function() {
    if (this.scanIntervalId_ !== null) {
      window.clearInterval(this.scanIntervalId_);
    }
  },

  /**
   * Returns network list object for testing.
   */
  getNetworkListForTest: function() {
    return this.$.networkList.$$('#networkList');
  },

  focus: function() {
    this.$.networkList.focus();
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.refreshNetworks();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    this.refreshNetworks();
  },

  /**
   * Requests the device and network states. May be called externally to force a
   * refresh and list update (e.g. when the element is shown).
   */
  refreshNetworks: function() {
    chrome.networkingPrivate.getDeviceStates(
        this.getDeviceStatesCallback_.bind(this));
  },

  /**
   * Returns default network if it is present.
   * @return {!CrOnc.NetworkStateProperties|undefined}
   */
  getDefaultNetwork: function() {
    let defaultNetwork;
    for (let i = 0; i < this.networkStateList_.length; ++i) {
      const state = this.networkStateList_[i];
      if (state.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
        defaultNetwork = state;
        break;
      }
      if (state.ConnectionState == CrOnc.ConnectionState.CONNECTING &&
          !defaultNetwork) {
        defaultNetwork = state;
        // Do not break here in case a non WiFi network is connecting but a
        // WiFi network is connected.
      } else if (state.Type == CrOnc.Type.WI_FI) {
        break;  // Non connecting or connected WiFI networks are always last.
      }
    }
    return defaultNetwork;
  },

  /**
   * Returns network with specified GUID if it is available.
   * @param {string} guid of network.
   * @return {!CrOnc.NetworkStateProperties|undefined}
   */
  getNetwork: function(guid) {
    return this.networkStateList_.find(function(network) {
      return network.GUID == guid;
    });
  },

  /**
   * @param {!Array<!CrOnc.DeviceStateProperties>} deviceStates
   * @private
   */
  getDeviceStatesCallback_: function(deviceStates) {
    this.isScanOngoing_ =
        deviceStates.some((deviceState) => !!deviceState.Scanning);

    const filter = {
      networkType: chrome.networkingPrivate.NetworkType.ALL,
      visible: true,
      configured: false
    };
    chrome.networkingPrivate.getNetworks(filter, function(networkStates) {
      this.getNetworksCallback_(deviceStates, networkStates);
    }.bind(this));
  },

  /**
   * @param {!Array<!CrOnc.DeviceStateProperties>} deviceStates
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStates
   * @private
   */
  getNetworksCallback_: function(deviceStates, networkStates) {
    this.cellularDeviceState_ = deviceStates.find(function(device) {
      return device.Type == CrOnc.Type.CELLULAR;
    });
    if (this.cellularDeviceState_) {
      this.ensureCellularNetwork_(networkStates);
    }
    this.networkStateList_ = networkStates;
    this.fire('network-list-changed', networkStates);

    const defaultNetwork = this.getDefaultNetwork();

    if ((!defaultNetwork && !this.defaultNetworkState_) ||
        (defaultNetwork && this.defaultNetworkState_ &&
         defaultNetwork.GUID == this.defaultNetworkState_.GUID &&
         defaultNetwork.ConnectionState ==
             this.defaultNetworkState_.ConnectionState)) {
      return;  // No change to network or ConnectionState
    }
    this.defaultNetworkState_ = defaultNetwork ?
        /** @type {!CrOnc.NetworkStateProperties|undefined} */ (
            Object.assign({}, defaultNetwork)) :
        undefined;
    this.fire('default-network-changed', defaultNetwork);
  },

  /**
   * Modifies |networkStates| to include a cellular network if one is required
   * but does not exist.
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStates
   * @private
   */
  ensureCellularNetwork_: function(networkStates) {
    if (networkStates.find(function(network) {
          return network.Type == CrOnc.Type.CELLULAR;
        })) {
      return;
    }
    let connectionState;
    switch (this.cellularDeviceState_.State) {
      case CrOnc.DeviceState.DISABLED:
      case CrOnc.DeviceState.PROHIBITED:
        return;  // No Cellular network
      case CrOnc.DeviceState.UNINITIALIZED:
      case CrOnc.DeviceState.ENABLING:
        // Leave |connectionState| undefined to indicate "initializing".
        break;
      case CrOnc.DeviceState.ENABLED:
        // The default network state is never connected. When a real network
        // state is provided, it will have ConnectionState properly set.
        connectionState = CrOnc.ConnectionState.NOT_CONNECTED;
        break;
    }
    // Add a Cellular network after the Ethernet network if it exists.
    const idx = networkStates.length > 0 &&
            networkStates[0].Type == CrOnc.Type.ETHERNET ?
        1 :
        0;
    const cellular = {
      GUID: '',
      Type: CrOnc.Type.CELLULAR,
      Cellular: {Scanning: this.cellularDeviceState_.Scanning},
      ConnectionState: connectionState,
    };
    networkStates.splice(idx, 0, cellular);
  },

  /**
   * Event triggered when a cr-network-list-item is selected.
   * @param {!{target: HTMLElement, detail: !CrOnc.NetworkStateProperties}} e
   * @private
   */
  onNetworkListItemSelected_: function(e) {
    const state = e.detail;
    e.target.blur();

    this.fire('network-item-selected', state);
  },
});
