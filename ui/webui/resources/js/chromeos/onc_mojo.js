// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities supporting network_config.mojom types. The strings
 * returned here should only be used for compatibility with the
 * networkingPrivate extension API and for debugging, they are not intended to
 * be user facing.
 */

class OncMojo {
  /**
   * @param {number|undefined} value
   * @return {string}
   */
  static getEnumString(value) {
    if (value === undefined) {
      return 'undefined';
    }
    return value.toString();
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ActivationStateType} value
   * @return {string}
   */
  static getActivationStateTypeString(value) {
    const ActivationStateType =
        chromeos.networkConfig.mojom.ActivationStateType;
    switch (value) {
      case ActivationStateType.kUnknown:
        return 'Unknown';
      case ActivationStateType.kNotActivated:
        return 'NotActivated';
      case ActivationStateType.kActivating:
        return 'Activating';
      case ActivationStateType.kPartiallyActivated:
        return 'PartiallyActivated';
      case ActivationStateType.kActivated:
        return 'Activated';
      case ActivationStateType.kNoService:
        return 'NoService';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.ActivationStateType}
   */
  static getActivationStateTypeFromString(value) {
    const ActivationStateType =
        chromeos.networkConfig.mojom.ActivationStateType;
    switch (value) {
      case 'Unknown':
        return ActivationStateType.kUnknown;
      case 'NotActivated':
        return ActivationStateType.kNotActivated;
      case 'Activating':
        return ActivationStateType.kActivating;
      case 'PartiallyActivated':
        return ActivationStateType.kPartiallyActivated;
      case 'Activated':
        return ActivationStateType.kActivated;
      case 'NoService':
        return ActivationStateType.kNoService;
    }
    assertNotReached('Unexpected value: ' + value);
    return ActivationStateType.kUnknown;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ConnectionStateType} value
   * @return {string}
   */
  static getConnectionStateTypeString(value) {
    const ConnectionStateType =
        chromeos.networkConfig.mojom.ConnectionStateType;
    switch (value) {
      case ConnectionStateType.kOnline:
        return 'Online';
      case ConnectionStateType.kConnected:
        return 'Connected';
      case ConnectionStateType.kPortal:
        return 'Portal';
      case ConnectionStateType.kConnecting:
        return 'Connecting';
      case ConnectionStateType.kNotConnected:
        return 'NotConnected';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.ConnectionStateType}
   */
  static getConnectionStateTypeFromString(value) {
    const ConnectionStateType =
        chromeos.networkConfig.mojom.ConnectionStateType;
    switch (value) {
      case 'Online':
        return ConnectionStateType.kOnline;
      case 'Connected':
        return ConnectionStateType.kConnected;
      case 'Portal':
        return ConnectionStateType.kPortal;
      case 'Connecting':
        return ConnectionStateType.kConnecting;
      case 'NotConnected':
        return ConnectionStateType.kNotConnected;
    }
    assertNotReached('Unexpected value: ' + value);
    return ConnectionStateType.kNotConnected;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ConnectionStateType} value
   * @return {boolean}
   */
  static connectionStateIsConnected(value) {
    const ConnectionStateType =
        chromeos.networkConfig.mojom.ConnectionStateType;
    switch (value) {
      case ConnectionStateType.kOnline:
      case ConnectionStateType.kConnected:
      case ConnectionStateType.kPortal:
        return true;
      case ConnectionStateType.kConnecting:
      case ConnectionStateType.kNotConnected:
        return false;
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return false;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.DeviceStateType} value
   * @return {string}
   */
  static getDeviceStateTypeString(value) {
    const DeviceStateType = chromeos.networkConfig.mojom.DeviceStateType;
    switch (value) {
      case DeviceStateType.kUninitialized:
        return 'Uninitialized';
      case DeviceStateType.kDisabled:
        return 'Disabled';
      case DeviceStateType.kEnabling:
        return 'Enabling';
      case DeviceStateType.kEnabled:
        return 'Enabled';
      case DeviceStateType.kProhibited:
        return 'Prohibited';
      case DeviceStateType.kUnavailable:
        return 'Unavailable';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.DeviceStateType}
   */
  static getDeviceStateTypeFromString(value) {
    const DeviceStateType = chromeos.networkConfig.mojom.DeviceStateType;
    switch (value) {
      case 'Uninitialized':
        return DeviceStateType.kUninitialized;
      case 'Disabled':
        return DeviceStateType.kDisabled;
      case 'Enabling':
        return DeviceStateType.kEnabling;
      case 'Enabled':
        return DeviceStateType.kEnabled;
      case 'Prohibited':
        return DeviceStateType.kProhibited;
      case 'Unavailable':
        return DeviceStateType.kUnavailable;
    }
    assertNotReached('Unexpected value: ' + value);
    return DeviceStateType.kUnavailable;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} value
   * @return {string}
   */
  static getNetworkTypeString(value) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    switch (value) {
      case NetworkType.kAll:
        return 'All';
      case NetworkType.kCellular:
        return 'Cellular';
      case NetworkType.kEthernet:
        return 'Ethernet';
      case NetworkType.kMobile:
        return 'Mobile';
      case NetworkType.kTether:
        return 'Tether';
      case NetworkType.kVPN:
        return 'VPN';
      case NetworkType.kWireless:
        return 'Wireless';
      case NetworkType.kWiFi:
        return 'WiFi';
      case NetworkType.kWiMAX:
        return 'WiMAX';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} value
   * @return {boolean}
   */
  static networkTypeIsMobile(value) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    switch (value) {
      case NetworkType.kCellular:
      case NetworkType.kMobile:
      case NetworkType.kTether:
        return true;
      case NetworkType.kAll:
      case NetworkType.kEthernet:
      case NetworkType.kVPN:
      case NetworkType.kWireless:
      case NetworkType.kWiFi:
      case NetworkType.kWiMAX:
        return false;
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return false;
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.NetworkType}
   */
  static getNetworkTypeFromString(value) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    switch (value) {
      case 'All':
        return NetworkType.kAll;
      case 'Cellular':
        return NetworkType.kCellular;
      case 'Ethernet':
        return NetworkType.kEthernet;
      case 'Mobile':
        return NetworkType.kMobile;
      case 'Tether':
        return NetworkType.kTether;
      case 'VPN':
        return NetworkType.kVPN;
      case 'Wireless':
        return NetworkType.kWireless;
      case 'WiFi':
        return NetworkType.kWiFi;
      case 'WiMAX':
        return NetworkType.kWiMAX;
    }
    assertNotReached('Unexpected value: ' + value);
    return NetworkType.kAll;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.OncSource} value
   * @return {string}
   */
  static getOncSourceString(value) {
    const OncSource = chromeos.networkConfig.mojom.OncSource;
    switch (value) {
      case OncSource.kNone:
        return 'None';
      case OncSource.kDevice:
        return 'Device';
      case OncSource.kDevicePolicy:
        return 'DevicePolicy';
      case OncSource.kUser:
        return 'User';
      case OncSource.kUserPolicy:
        return 'UserPolicy';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.OncSource} value
   */
  static getOncSourceFromString(value) {
    const OncSource = chromeos.networkConfig.mojom.OncSource;
    switch (value) {
      case CrOnc.Source.NONE:
        return OncSource.kNone;
      case CrOnc.Source.DEVICE:
        return OncSource.kDevice;
      case CrOnc.Source.DEVICE_POLICY:
        return OncSource.kDevicePolicy;
      case CrOnc.Source.USER:
        return OncSource.kUser;
      case CrOnc.Source.USER_POLICY:
        return OncSource.kUserPolicy;
    }
    assertNotReached('Unexpected value: ' + value);
    return OncSource.kNone;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.SecurityType} value
   * @return {string}
   */
  static getSecurityTypeString(value) {
    const SecurityType = chromeos.networkConfig.mojom.SecurityType;
    switch (value) {
      case SecurityType.kNone:
        return 'None';
      case SecurityType.kWep8021x:
        return 'WEP-8021X';
      case SecurityType.kWepPsk:
        return 'WEP-PSK';
      case SecurityType.kWpaEap:
        return 'WPA-EAP';
      case SecurityType.kWpaPsk:
        return 'WPA-PSK';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.SecurityType}
   */
  static getSecurityTypeFromString(value) {
    const SecurityType = chromeos.networkConfig.mojom.SecurityType;
    switch (value) {
      case 'None':
        return SecurityType.kNone;
      case 'WEP-8021X':
        return SecurityType.kWep8021x;
      case 'WEP-PSK':
        return SecurityType.kWepPsk;
      case 'WPA-EAP':
        return SecurityType.kWpaEap;
      case 'WPA-PSK':
        return SecurityType.kWpaPsk;
    }
    assertNotReached('Unexpected value: ' + value);
    return SecurityType.kNone;
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.VPNType}
   */
  static getVPNTypeFromString(value) {
    const VPNType = chromeos.networkConfig.mojom.VPNType;
    switch (value) {
      case 'L2TP-IPsec':
        return VPNType.kL2TPIPsec;
      case 'OpenVPN':
        return VPNType.kOpenVPN;
      case 'ThirdPartyVPN':
        return VPNType.kThirdPartyVPN;
      case 'ARCVPN':
        return VPNType.kArcVPN;
    }
    assertNotReached('Unexpected value: ' + value);
    return VPNType.kOpenVPN;
  }

  /**
   * This infers the type from |key|, casts |value| (which should be a number)
   * to the corresponding enum type, and converts it to a string. If |key| is
   * known, then |value| is expected to match an enum value. Otherwise |value|
   * is simply returned.
   * @param {string} key
   * @param {number|string} value
   * @return {number|string}
   */
  static getTypeString(key, value) {
    if (key == 'activationState') {
      return OncMojo.getActivationStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.ActivationStateType} */ (
              value));
    }
    if (key == 'connectionState') {
      return OncMojo.getConnectionStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.ConnectionStateType} */ (
              value));
    }
    if (key == 'deviceState') {
      return OncMojo.getDeviceStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.DeviceStateType} */ (value));
    }
    if (key == 'type') {
      return OncMojo.getNetworkTypeString(
          /** @type {!chromeos.networkConfig.mojom.NetworkType} */ (value));
    }
    if (key == 'source') {
      return OncMojo.getOncSourceString(
          /** @type {!chromeos.networkConfig.mojom.OncSource} */ (value));
    }
    if (key == 'security') {
      return OncMojo.getSecurityTypeString(
          /** @type {!chromeos.networkConfig.mojom.SecurityType} */ (value));
    }
    return value;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network
   * @return {string}
   */
  static getNetworkDisplayName(network) {
    if (!network.name) {
      assert(CrOncStrings);
      const typestr = 'OncType' + OncMojo.getNetworkTypeString(network.type);
      return CrOncStrings[typestr];
    }
    if (network.type == chromeos.networkConfig.mojom.NetworkType.kVPN) {
      const vpnType = network.vpn.type;
      if (vpnType == chromeos.networkConfig.mojom.VPNType.kThirdPartyVPN) {
        const providerName = network.vpn.providerName;
        if (providerName) {
          assert(CrOncStrings);
          return CrOncStrings.vpnNameTemplate.replace('$1', providerName)
              .replace('$2', network.name);
        }
      }
    }
    return network.name;
  }

  /**
   * Gets the SignalStrength value from |network| based on network.type.
   * @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network
   * @return {number} The signal strength value if it exists or 0.
   */
  static getSignalStrength(network) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    switch (network.type) {
      case NetworkType.kCellular:
        return network.cellular.signalStrength;
      case NetworkType.kTether:
        return network.tether.signalStrength;
      case NetworkType.kWiFi:
        return network.wifi.signalStrength;
      case NetworkType.kWiMAX:
        return network.wimax.signalStrength;
    }
    assertNotReached();
    return 0;
  }

  /**
   * Returns a NetworkStateProperties object with type set and default values.
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @return {chromeos.networkConfig.mojom.NetworkStateProperties}
   */
  static getDefaultNetworkState(type) {
    const mojom = chromeos.networkConfig.mojom;
    const result = {
      connectable: false,
      connectRequested: false,
      connectionState: mojom.ConnectionStateType.kNotConnected,
      guid: '',
      name: '',
      priority: 0,
      proxyMode: mojom.ProxyMode.kDirect,
      prohibitedByPolicy: false,
      source: mojom.OncSource.kNone,
      type: type,
    };
    switch (type) {
      case mojom.NetworkType.kCellular:
        result.cellular = {
          activationState: mojom.ActivationStateType.kUnknown,
          networkTechnology: '',
          roaming: false,
          signalStrength: 0,
        };
        break;
      case mojom.NetworkType.kEthernet:
        result.ethernet = {
          authentication: mojom.AuthenticationType.kNone,
        };
        break;
      case mojom.NetworkType.kTether:
        result.tether = {
          batteryPercentage: 0,
          carrier: '',
          hasConnectedToHost: false,
          signalStrength: 0,
        };
        break;
      case mojom.NetworkType.kVPN:
        result.vpn = {
          type: mojom.VPNType.kOpenVPN,
          providerId: '',
          providerName: '',
        };
        break;
      case mojom.NetworkType.kWiFi:
        result.wifi = {
          bssid: '',
          frequency: 0,
          hexSsid: '',
          security: mojom.SecurityType.kNone,
          signalStrength: 0,
          ssid: '',
        };
        break;
      case mojom.NetworkType.kWiMAX:
        result.wimax = {
          signalStrength: 0,
        };
        break;
    }
    return result;
  }

  /**
   * Converts an ONC dictionary to NetworkStateProperties. See onc_spec.md
   * for the dictionary spec.
   * @param {!CrOnc.NetworkProperties} properties
   * @return {chromeos.networkConfig.mojom.NetworkStateProperties}
   */
  static oncPropertiesToNetworkState(properties) {
    const mojom = chromeos.networkConfig.mojom;
    const networkState = OncMojo.getDefaultNetworkState(
        OncMojo.getNetworkTypeFromString(properties.Type));
    networkState.connectable = !!properties.Connectable;
    if (properties.ConnectionState) {
      networkState.connectionState =
          OncMojo.getConnectionStateTypeFromString(properties.ConnectionState);
    }
    networkState.guid = properties.GUID;
    networkState.name = CrOnc.getStateOrActiveString(properties.Name);
    if (properties.Priority) {
      const priority = /** @type {number|undefined} */ (
          CrOnc.getActiveValue(properties.Priority));
      if (priority !== undefined) {
        networkState.priority = priority;
      }
    }
    if (properties.Source) {
      networkState.source = OncMojo.getOncSourceFromString(properties.Source);
    }

    switch (networkState.type) {
      case mojom.NetworkType.kCellular:
        if (properties.Cellular) {
          networkState.cellular.activationState =
              OncMojo.getActivationStateTypeFromString(
                  properties.Cellular.ActivationState || 'Unknown');
          networkState.cellular.networkTechnology =
              properties.Cellular.NetworkTechnology || '';
          networkState.cellular.roaming =
              properties.Cellular.RoamingState == CrOnc.RoamingState.ROAMING;
          networkState.cellular.signalStrength =
              properties.Cellular.SignalStrength || 0;
        }
        break;
      case mojom.NetworkType.kEthernet:
        if (properties.Ethernet) {
          networkState.ethernet.authentication =
              properties.Ethernet.Authentication ==
                  CrOnc.Authentication.WEP_8021X ?
              mojom.AuthenticationType.k8021x :
              mojom.AuthenticationType.kNone;
        }
        break;
      case mojom.NetworkType.kTether:
        if (properties.Tether) {
          networkState.tether.batteryPercentage =
              properties.Tether.BatteryPercentage || 0;
          networkState.tether.carrier =
              properties.Tether.NetworkTechnology || '';
          networkState.tether.hasConnectedToHost =
              properties.Tether.HasConnectedToHost;
          networkState.tether.signalStrength =
              properties.Tether.SignalStrength || 0;
        }
        break;
      case mojom.NetworkType.kVPN:
        if (properties.VPN) {
          networkState.vpn.providerName =
              (properties.VPN.ThirdPartyVPN &&
               properties.VPN.ThirdPartyVPN.ProviderName) ||
              '';
          networkState.vpn.vpnType = OncMojo.getVPNTypeFromString(
              CrOnc.getStateOrActiveString(properties.VPN.Type) ||
              CrOnc.VPNType.OPEN_VPN);
        }
        break;
      case mojom.NetworkType.kWiFi:
        if (properties.WiFi) {
          networkState.wifi.bssid = properties.WiFi.BSSID || '';
          networkState.wifi.frequency = properties.WiFi.Frequency || 0;
          networkState.wifi.hexSsid =
              CrOnc.getStateOrActiveString(properties.WiFi.HexSSID);
          networkState.wifi.security = OncMojo.getSecurityTypeFromString(
              CrOnc.getStateOrActiveString(properties.WiFi.Security) ||
              CrOnc.Security.NONE);
          networkState.wifi.signalStrength =
              properties.WiFi.SignalStrength || 0;
          networkState.wifi.ssid =
              CrOnc.getStateOrActiveString(properties.WiFi.SSID);
        }
        break;
      case mojom.NetworkType.kWiMAX:
        if (properties.WiMAX) {
          networkState.wimax.signalStrength =
              properties.WiMAX.SignalStrength || 0;
        }
        break;
    }
    return networkState;
  }
}

/** @typedef {chromeos.networkConfig.mojom.DeviceStateProperties} */
OncMojo.DeviceStateProperties;

/** @typedef {chromeos.networkConfig.mojom.NetworkStateProperties} */
OncMojo.NetworkStateProperties;
