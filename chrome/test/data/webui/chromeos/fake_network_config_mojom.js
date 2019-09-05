// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of CrosNetworkConfig for testing.
 * Currently this is built on top of FakeNetworkingPrivate to avoid
 * inconsistencies between fakes in tests.
 */

// TODO(stevenjb): Include cros_network_config.mojom.js and extend
// CrosNetworkConfigInterface
class FakeNetworkConfig {
  /** @param {!NetworkingPrivate} extensionApi */
  constructor(extensionApi) {
    this.extensionApi_ = extensionApi;

    this.observers_ = [];
    extensionApi.onNetworkListChanged.addListener(
        this.onNetworkListChanged.bind(this));
    extensionApi.onDeviceStateListChanged.addListener(
        this.onDeviceStateListChanged.bind(this));
    extensionApi.onActiveNetworksChanged.addListener(
        this.onActiveNetworksChanged.bind(this));

    this.vpnProviders_ =
        /** @type {!Array<!chromeos.networkConfig.mojom.VpnProvider>}*/ ([]);
  }

  onNetworkListChanged(networks) {
    this.observers_.forEach(o => o.onNetworkStateListChanged());
  }

  onDeviceStateListChanged() {
    this.observers_.forEach(o => o.onDeviceStateListChanged());
  }

  /**
   * @param {?Array<?chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   */
  onActiveNetworksChanged(networks) {
    this.observers_.forEach(o => o.onActiveNetworksChanged(networks));
  }

  onVpnProvidersChanged() {
    this.observers_.forEach(o => o.onVpnProvidersChanged());
  }

  /**
   * @param { !chromeos.networkConfig.mojom.CrosNetworkConfigObserverProxy }
   *     observer
   */
  addObserver(observer) {
    this.observers_.push(observer);
  }

  /**
   * @param {string} guid
   * @return {!Promise<{result:
   *     !chromeos.networkConfig.mojom.NetworkStateProperties>>}
   */
  getNetworkState(guid) {
    return new Promise(resolve => {
      this.extensionApi_.getState(guid, network => {
        const result = network ? this.networkStateToMojo_(network) : null;
        resolve({result: result});
      });
    });
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkFilter} filter
   * @return {!Promise<{result:
   *     !Array<!chromeos.networkConfig.mojom.NetworkStateProperties>}>}
   */
  getNetworkStateList(filter) {
    return new Promise(resolve => {
      const extensionFilter = {
        networkType: OncMojo.getNetworkTypeString(filter.networkType)
      };
      this.extensionApi_.getNetworks(extensionFilter, networks => {
        const result =
            networks.map(network => this.networkStateToMojo_(network));
        resolve({result: result});
      });
    });
  }

  /**
   * @return {!Promise<{result:
   *     !Array<!chromeos.networkConfig.mojom.DeviceStateProperties>}>}
   */
  getDeviceStateList() {
    return new Promise(resolve => {
      this.extensionApi_.getDeviceStates(devices => {
        const result = devices.map(device => this.deviceToMojo(device));
        resolve({result: result});
      });
    });
  }

  /**
   * @param {string} guid
   * @return {!Promise<{result:
   *     !chromeos.networkConfig.mojom.ManagedProperties}>}
   */
  getManagedProperties(guid) {
    return new Promise(resolve => {
      this.extensionApi_.getManagedProperties(guid, network => {
        const result = network ? this.managedPropertiesToMojo_(network) : null;
        resolve({result: result});
      });
    });
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @param {boolean} enabled
   * @return {!Promise<{success: boolean}>}
   */
  setNetworkTypeEnabledState(type, enabled) {
    if (enabled) {
      this.extensionApi_.enableNetworkType(OncMojo.getNetworkTypeString(type));
    } else {
      this.extensionApi_.disableNetworkType(OncMojo.getNetworkTypeString(type));
    }
    return Promise.resolve(true);
  }

  /** @param { !chromeos.networkConfig.mojom.NetworkType } type */
  requestNetworkScan(type) {
    this.extensionApi_.requestNetworkScan();
  }

  /**
   * @return { !Promise<{result: !chromeos.networkConfig.mojom.GlobalPolicy}>}
   */
  getGlobalPolicy() {
    return new Promise(resolve => {
      this.extensionApi_.getGlobalPolicy(globalPolicy => {
        const result = {
          allowOnlyPolicyNetworksToAutoconnect:
              globalPolicy.AllowOnlyPolicyNetworksToAutoconnect,
          allowOnlyPolicyNetworksToConnect:
              globalPolicy.AllowOnlyPolicyNetworksToConnect,
          allowOnlyPolicyNetworksToConnectIfAvailable:
              globalPolicy.AllowOnlyPolicyNetworksToConnectIfAvailable,
          blacklistedHexSsids: globalPolicy.BlacklistedHexSsids,
        };
        resolve({result: result});
      });
    });
  }

  /**
   * @return { !Promise<{
   *     result: !Array<!chromeos.networkConfig.mojom.VpnProvider>}>}
   */
  getVpnProviders() {
    return new Promise(resolve => {
      resolve({providers: this.vpnProviders_});
    });
  }

  /**
   * @param {string} type
   * @return {?chrome.networkingPrivate.DeviceStateProperties}
   */
  getDeviceStateForTest(type) {
    return this.deviceToMojo(this.extensionApi_.getDeviceStateForTest(type));
  }

  /** @param {!Array<!chromeos.networkConfig.mojom.VpnProvider>} providers */
  setVpnProvidersForTest(providers) {
    this.vpnProviders_ = providers;
    this.onVpnProvidersChanged();
  }

  /**
   * @param {!chrome.networkingPrivate.DeviceStateProperties} device
   * @return {!chromeos.networkConfig.mojom.DeviceStateProperties}
   */
  deviceToMojo(device) {
    return {
      deviceState: OncMojo.getDeviceStateTypeFromString(device.State),
      type: OncMojo.getNetworkTypeFromString(device.Type),
    };
  }

  /**
   * @param {!chrome.networkingPrivate.NetworkStateProperties} network
   * @return {!chromeos.networkConfig.mojom.NetworkStateProperties}
   * @private
   */
  networkStateToMojo_(network) {
    const mojoNetwork = OncMojo.getDefaultNetworkState(
        OncMojo.getNetworkTypeFromString(network.Type));
    mojoNetwork.guid = network.GUID;
    mojoNetwork.name = network.Name;
    if (network.ConnectionState) {
      mojoNetwork.connectionState =
          OncMojo.getConnectionStateTypeFromString(network.ConnectionState);
    }
    if (network.VPN) {
      mojoNetwork.vpn = {type: OncMojo.getVpnTypeFromString(network.VPN.Type)};
      if (network.VPN.ThirdPartyVPN) {
        mojoNetwork.vpn.providerId = network.VPN.ThirdPartyVPN.ExtensionID;
        mojoNetwork.vpn.providerName = network.VPN.ThirdPartyVPN.ProviderName;
      }
    }
    return mojoNetwork;
  }

  /**
   * @param {!chrome.networkingPrivate.ManagedProperties} network
   * @return {!chromeos.networkConfig.mojom.ManagedProperties}
   * @private
   */
  managedPropertiesToMojo_(network) {
    const mojoNetwork = OncMojo.getDefaultManagedProperties(
        OncMojo.getNetworkTypeFromString(network.Type), network.GUID,
        CrOnc.getActiveValue(network.Name));
    mojoNetwork.source = network.Source ?
        OncMojo.getOncSourceFromString(network.Source) :
        chromeos.networkConfig.mojom.OncSource.kNone;
    if (network.ConnectionState) {
      mojoNetwork.connectionState =
          OncMojo.getConnectionStateTypeFromString(network.ConnectionState);
    }
    if (network.WiFi) {
      mojoNetwork.wifi = {
        frequency: network.WiFi.Frequency,
        ssid: OncMojo.createManagedString(
            CrOnc.getActiveValue(network.WiFi.SSID)),
        security: OncMojo.getSecurityTypeFromString(
            CrOnc.getActiveValue(network.WiFi.Security)),
        signalStrength: network.WiFi.SignalStrength,
      };
      if (network.WiFi.AutoConnect) {
        mojoNetwork.wifi.autoConnect = OncMojo.createManagedBool(
            CrOnc.getActiveValue(network.WiFi.AutoConnect));
      }
    }
    return mojoNetwork;
  }
}
