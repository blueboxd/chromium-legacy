// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the network state for a specific
 * type and a list of networks for that type. NOTE: It both Cellular and Tether
 * technologies are available, they are combined into a single 'Mobile data'
 * section. See crbug.com/726380.
 */

(function() {

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'network-summary-item',

  behaviors: [CrPolicyNetworkBehavior, I18nBehavior],

  properties: {
    /**
     * Device state for the network type. This might briefly be undefined if a
     * device becomes unavailable.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    deviceState: Object,

    /**
     * If both Cellular and Tether technologies exist, we combine the sections
     * and set this to the device state for Tether.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    tetherDeviceState: Object,

    /**
     * Network state for the active network.
     * @type {!CrOnc.NetworkStateProperties|undefined}
     */
    activeNetworkState: Object,

    /**
     * List of all network state data for the network type.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Title line describing the network type to appear in the row's top line.
     * If it is undefined, the title text is a default from CrOncStrings (see
     * this.getTitleText_() below).
     * @type {string|undefined}
     */
    networkTitleText: String,

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

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getNetworkStateText_: function(activeNetworkState, deviceState) {
    const stateText = this.getConnectionStateText_(activeNetworkState);
    if (stateText) {
      return stateText;
    }
    // No network state, use device state.
    if (deviceState) {
      // Type specific scanning or initialization states.
      if (deviceState.type == mojom.NetworkType.kCellular) {
        if (deviceState.scanning) {
          return this.i18n('internetMobileSearching');
        }
        if (deviceState.deviceState == mojom.DeviceStateType.kUninitialized) {
          return this.i18n('internetDeviceInitializing');
        }
      } else if (deviceState.type == mojom.NetworkType.kTether) {
        if (deviceState.deviceState == mojom.DeviceStateType.kUninitialized) {
          return this.i18n('tetherEnableBluetooth');
        }
      }
      // Enabled or enabling states.
      if (deviceState.deviceState == mojom.DeviceStateType.kEnabled) {
        if (this.networkStateList.length > 0) {
          return CrOncStrings.networkListItemNotConnected;
        }
        return CrOncStrings.networkListItemNoNetwork;
      }
      if (deviceState.deviceState == mojom.DeviceStateType.kEnabling) {
        return this.i18n('internetDeviceEnabling');
      }
    }
    // No device or unknown device state, use 'off'.
    return this.i18n('deviceOff');
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} networkState
   * @return {string}
   * @private
   */
  getConnectionStateText_: function(networkState) {
    const state = networkState ? networkState.ConnectionState : null;
    if (!state) {
      return '';
    }
    const name = CrOnc.getNetworkName(networkState);
    switch (state) {
      case CrOnc.ConnectionState.CONNECTED:
        return name;
      case CrOnc.ConnectionState.CONNECTING:
        if (name) {
          return CrOncStrings.networkListItemConnectingTo.replace('$1', name);
        }
        return CrOncStrings.networkListItemConnecting;
      case CrOnc.ConnectionState.NOT_CONNECTED:
        if (networkState.Type == mojom.NetworkType.kCellular &&
            networkState.Cellular && networkState.Cellular.Scanning) {
          return this.i18n('internetMobileSearching');
        }
        return CrOncStrings.networkListItemNotConnected;
    }
    assertNotReached();
    return state;
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @return {boolean}
   * @private
   */
  showPolicyIndicator_: function(activeNetworkState) {
    return (activeNetworkState !== undefined &&
            activeNetworkState.ConnectionState ==
                CrOnc.ConnectionState.CONNECTED) ||
        this.isPolicySource(activeNetworkState.Source);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  showSimInfo_: function(deviceState) {
    if (!deviceState || deviceState.type != mojom.NetworkType.kCellular) {
      return false;
    }
    return this.simLockedOrAbsent_(deviceState);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  simLockedOrAbsent_: function(deviceState) {
    if (this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    if (!deviceState.simLockStatus) {
      return false;
    }
    if (deviceState.simLockStatus.simAbsent) {
      return true;
    }
    const simLockType = deviceState.simLockStatus.lockType;
    return simLockType == CrOnc.LockType.PIN ||
        simLockType == CrOnc.LockType.PUK;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return !!deviceState &&
        deviceState.deviceState == mojom.DeviceStateType.kEnabled;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_: function(deviceState) {
    if (!deviceState) {
      return false;
    }
    switch (deviceState.type) {
      case mojom.NetworkType.kEthernet:
      case mojom.NetworkType.kVPN:
        return false;
      case mojom.NetworkType.kTether:
        return true;
      case mojom.NetworkType.kWiFi:
      case mojom.NetworkType.kWiMAX:
        return deviceState.deviceState != mojom.DeviceStateType.kUninitialized;
      case mojom.NetworkType.kCellular:
        return deviceState.deviceState !=
            mojom.DeviceStateType.kUninitialized &&
            !this.simLockedOrAbsent_(deviceState);
    }
    assertNotReached();
    return false;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_: function(deviceState) {
    return this.enableToggleIsVisible_(deviceState) &&
        deviceState.deviceState != mojom.DeviceStateType.kProhibited &&
        deviceState.deviceState != mojom.DeviceStateType.kUninitialized;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getToggleA11yString_: function(deviceState) {
    if (!this.enableToggleIsVisible_(deviceState)) {
      return '';
    }
    switch (deviceState.type) {
      case mojom.NetworkType.kTether:
      case mojom.NetworkType.kCellular:
        return this.i18n('internetToggleMobileA11yLabel');
      case mojom.NetworkType.kWiFi:
        return this.i18n('internetToggleWiFiA11yLabel');
      case mojom.NetworkType.kWiMAX:
        return this.i18n('internetToggleWiMAXA11yLabel');
    }
    assertNotReached();
    return '';
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  showDetailsIsVisible_: function(
      activeNetworkState, deviceState, networkStateList) {
    return this.deviceIsEnabled_(deviceState) &&
        (!!activeNetworkState.GUID || networkStateList.length > 0);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowSubpage_: function(deviceState, networkStateList) {
    if (!deviceState) {
      return false;
    }
    const type = deviceState.type;
    if (type == mojom.NetworkType.kTether ||
        (type == mojom.NetworkType.kCellular && this.tetherDeviceState)) {
      // The "Mobile data" subpage should always be shown if Tether networks are
      // available, even if there are currently no associated networks.
      return true;
    }
    const minlen =
        (type == mojom.NetworkType.kWiFi || type == mojom.NetworkType.kVPN) ?
        1 :
        2;
    return networkStateList.length >= minlen;
  },

  /**
   * @param {!Event} event The enable button event.
   * @private
   */
  onShowDetailsTap_: function(event) {
    if (!this.deviceIsEnabled_(this.deviceState)) {
      if (this.enableToggleIsEnabled_(this.deviceState)) {
        const type = this.deviceState.type;
        this.fire('device-enabled-toggled', {enabled: true, type: type});
      }
    } else if (this.shouldShowSubpage_(
                   this.deviceState, this.networkStateList)) {
      this.fire('show-networks', this.deviceState.type);
    } else if (this.activeNetworkState.GUID) {
      this.fire('show-detail', this.activeNetworkState);
    } else if (this.networkStateList.length > 0) {
      this.fire('show-detail', this.networkStateList[0]);
    }
    event.stopPropagation();
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {string}
   * @private
   */
  getDetailsA11yString_: function(
      activeNetworkState, deviceState, networkStateList) {
    if (!this.shouldShowSubpage_(deviceState, networkStateList)) {
      if (activeNetworkState.GUID) {
        return CrOnc.getNetworkName(activeNetworkState);
      } else if (networkStateList.length > 0) {
        return CrOnc.getNetworkName(networkStateList[0]);
      }
    }
    return this.getNetworkTypeString_(
        /** @type {!chrome.networkingPrivate.NetworkType} */ (
            OncMojo.getNetworkTypeString(deviceState.type)));
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_: function(event) {
    assert(this.deviceState);
    const deviceIsEnabled = this.deviceIsEnabled_(this.deviceState);
    this.fire(
        'device-enabled-toggled',
        {enabled: !deviceIsEnabled, type: this.deviceState.type});
  },

  /**
   * @return {string}
   * @private
   */
  getTitleText_: function() {
    assert(CrOncStrings);
    return this.networkTitleText ||
        this.getNetworkTypeString_(this.activeNetworkState.Type);
  },

  /**
   * Make sure events in embedded components do not propagate to onDetailsTap_.
   * @param {!Event} event
   * @private
   */
  doNothing_: function(event) {
    event.stopPropagation();
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkType} type
   * @return {string}
   * @private
   */
  getNetworkTypeString_: function(type) {
    // The shared Cellular/Tether subpage is referred to as "Mobile".
    // TODO(khorimoto): Remove once Cellular/Tether are split into their own
    // sections.
    if (type == CrOnc.Type.CELLULAR || type == CrOnc.Type.TETHER) {
      return this.i18n('OncTypeMobile');
    }
    return this.i18n('OncType' + type);
  },
});
})();
