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
    assertNotReached();
    return '';
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
    assertNotReached();
    return '';
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
    assertNotReached();
    return '';
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
    assertNotReached();
    return '';
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
    assertNotReached();
    return NetworkType.kAll;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ONCSource} value
   * @return {string}
   */
  static getONCSourceString(value) {
    const ONCSource = chromeos.networkConfig.mojom.ONCSource;
    switch (value) {
      case ONCSource.kUnknown:
        return 'Unknown';
      case ONCSource.kNone:
        return 'None';
      case ONCSource.kUserImport:
        return 'UserImport';
      case ONCSource.kDevicePolicy:
        return 'DevicePolicy';
      case ONCSource.kUserPolicy:
        return 'UserPolicy';
    }
    assertNotReached();
    return '';
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
        return 'Wep8021x';
      case SecurityType.kWepPsk:
        return 'WepPsk';
      case SecurityType.kWpaEap:
        return 'WpaEap';
      case SecurityType.kWpaPsk:
        return 'WpaPsk';
    }
    assertNotReached();
    return '';
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
      return this.getActivationStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.ActivationStateType} */ (
              value));
    }
    if (key == 'connectionState') {
      return this.getConnectionStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.ConnectionStateType} */ (
              value));
    }
    if (key == 'deviceState') {
      return this.getDeviceStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.DeviceStateType} */ (value));
    }
    if (key == 'type') {
      return this.getNetworkTypeString(
          /** @type {!chromeos.networkConfig.mojom.NetworkType} */ (value));
    }
    if (key == 'source') {
      return this.getONCSourceString(
          /** @type {!chromeos.networkConfig.mojom.ONCSource} */ (value));
    }
    if (key == 'security') {
      return this.getSecurityTypeString(
          /** @type {!chromeos.networkConfig.mojom.SecurityType} */ (value));
    }
    return value;
  }
}

/** @typedef {chromeos.networkConfig.mojom.DeviceStateProperties} */
OncMojo.DeviceStateProperties;

/** @typedef {chromeos.networkConfig.mojom.NetworkStateProperties} */
OncMojo.NetworkStateProperties;
