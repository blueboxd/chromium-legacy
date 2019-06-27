// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about WiFi,
 * WiMAX, or virtual networks.
 */

(function() {

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'settings-internet-subpage',

  behaviors: [
    CrNetworkListenerBehavior,
    CrPolicyNetworkBehavior,
    settings.RouteObserverBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * Highest priority connected network or null. Provided by
     * settings-internet-page (but set in network-summary).
     * @type {?OncMojo.NetworkStateProperties|undefined}
     */
    defaultNetwork: Object,

    /**
     * Device state for the network type. Note: when both Cellular and Tether
     * are available this will always be set to the Cellular device state and
     * |tetherDeviceState| will be set to the Tether device state.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    deviceState: Object,

    /**
     * If both Cellular and Tether technologies exist, we combine the subpages
     * and set this to the device state for Tether.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    tetherDeviceState: Object,

    /** @type {!chrome.networkingPrivate.GlobalPolicy|undefined} */
    globalPolicy: Object,

    /**
     * List of third party VPN providers.
     * @type
     *     {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>|undefined}
     */
    thirdPartyVpnProviders: Array,

    /**
     * List of Arc VPN providers.
     * @type {!Array<!settings.ArcVpnProvider>|undefined}
     */
    arcVpnProviders: Array,

    showSpinner: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /**
     * List of all network state data for the network type.
     * @private {!Array<!OncMojo.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Dictionary of lists of network states for third party VPNs.
     * @private {!Object<!Array<!OncMojo.NetworkStateProperties>>}
     */
    thirdPartyVpns_: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /**
     * Dictionary of lists of network states for Arc VPNs.
     * @private {!Object<!Array<!OncMojo.NetworkStateProperties>>}
     */
    arcVpns_: {
      type: Object,
      value: function() {
        return {};
      }
    },

    /**
     * List of potential Tether hosts whose "Google Play Services" notifications
     * are disabled (these notifications are required to use Instant Tethering).
     * @private {!Array<string>}
     */
    notificationsDisabledDeviceNames_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Whether to show technology badge on mobile network icons.
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

  observers: ['deviceStateChanged_(deviceState)'],

  /** @private {number|null} */
  scanIntervalId_: null,

  /** @private  {settings.InternetPageBrowserProxy} */
  browserProxy_: null,

  /**
   * This UI will use both the networkingPrivate extension API and the
   * networkConfig mojo API until we provide all of the required functionality
   * in networkConfig. TODO(stevenjb): Remove use of networkingPrivate api.
   * @private {?chromeos.networkConfig.mojom.CrosNetworkConfigProxy}
   */
  networkConfigProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.InternetPageBrowserProxyImpl.getInstance();
    this.networkConfigProxy_ =
        network_config.MojoInterfaceProviderImpl.getInstance()
            .getMojoServiceProxy();
  },

  /** @override */
  ready: function() {
    this.browserProxy_.setGmsCoreNotificationsDisabledDeviceNamesCallback(
        this.onNotificationsDisabledDeviceNamesReceived_.bind(this));
    this.browserProxy_.requestGmsCoreNotificationsDisabledDeviceNames();
  },

  /** override */
  detached: function() {
    this.stopScanning_();
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route != settings.routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }
    // Clear any stale data.
    this.networkStateList_ = [];
    this.thirdPartyVpns_ = {};
    this.arcVpns_ = {};
    // Request the list of networks and start scanning if necessary.
    this.getNetworkStateList_();
    this.updateScanning_();
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged: function(networks) {
    this.getNetworkStateList_();
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.getNetworkStateList_();
  },

  /** @private */
  deviceStateChanged_: function() {
    this.showSpinner =
        this.deviceState !== undefined && !!this.deviceState.scanning;

    // Scans should only be triggered by the "networks" subpage.
    if (settings.getCurrentRoute() != settings.routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }

    this.getNetworkStateList_();
    this.updateScanning_();
  },

  /** @private */
  updateScanning_: function() {
    if (!this.deviceState) {
      return;
    }

    if (this.shouldStartScan_()) {
      this.startScanning_();
      return;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldStartScan_: function() {
    // Scans should be kicked off from the Wi-Fi networks subpage.
    if (this.deviceState.type == mojom.NetworkType.kWiFi) {
      return true;
    }

    // Scans should be kicked off from the Mobile data subpage, as long as it
    // includes Tether networks.
    if (this.deviceState.type == mojom.NetworkType.kTether ||
        (this.deviceState.type == mojom.NetworkType.kCellular &&
         this.tetherDeviceState)) {
      return true;
    }

    return false;
  },

  /** @private */
  startScanning_: function() {
    if (this.scanIntervalId_ != null) {
      return;
    }
    const INTERVAL_MS = 10 * 1000;
    this.networkConfigProxy_.requestNetworkScan(this.deviceState.type);
    this.scanIntervalId_ = window.setInterval(() => {
      this.networkConfigProxy_.requestNetworkScan(this.deviceState.type);
    }, INTERVAL_MS);
  },

  /** @private */
  stopScanning_: function() {
    if (this.scanIntervalId_ == null) {
      return;
    }
    window.clearInterval(this.scanIntervalId_);
    this.scanIntervalId_ = null;
  },

  /** @private */
  getNetworkStateList_: function() {
    if (!this.deviceState) {
      return;
    }
    const filter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      limit: chromeos.networkConfig.mojom.kNoLimit,
      networkType: this.deviceState.type,
    };
    this.networkConfigProxy_.getNetworkStateList(filter).then(response => {
      this.onGetNetworks_(response.result);
    });
  },

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStates
   * @private
   */
  onGetNetworks_: function(networkStates) {
    if (!this.deviceState) {
      // Edge case when device states change before this callback.
      return;
    }

    // For the Cellular/Mobile subpage, also request Tether networks.
    if (this.deviceState.type == mojom.NetworkType.kCellular &&
        this.tetherDeviceState) {
      const filter = {
        filter: chromeos.networkConfig.mojom.FilterType.kVisible,
        limit: chromeos.networkConfig.mojom.kNoLimit,
        networkType: mojom.NetworkType.kTether,
      };
      this.networkConfigProxy_.getNetworkStateList(filter).then(response => {
        const tetherNetworkStates = response.result;
        this.networkStateList_ = networkStates.concat(tetherNetworkStates);
      });
      return;
    }

    // For VPNs, separate out third party VPNs and Arc VPNs.
    if (this.deviceState.type == mojom.NetworkType.kVPN) {
      const builtinNetworkStates = [];
      const thirdPartyVpns = {};
      const arcVpns = {};
      networkStates.forEach(state => {
        assert(state.type == mojom.NetworkType.kVPN);
        switch (state.vpn.type) {
          case mojom.VPNType.kL2TPIPsec:
          case mojom.VPNType.kOpenVPN:
            builtinNetworkStates.push(state);
            break;
          case mojom.VPNType.kThirdPartyVPN:
            const providerName = state.vpn.providerName;
            thirdPartyVpns[providerName] = thirdPartyVpns[providerName] || [];
            thirdPartyVpns[providerName].push(state);
            break;
          case mojom.VPNType.kArcVPN:
            const arcProviderName = this.get('VPN.Host', state);
            if (OncMojo.connectionStateIsConnected(state.connectionState)) {
              arcVpns[arcProviderName] = arcVpns[arcProviderName] || [];
              arcVpns[arcProviderName].push(state);
            }
            break;
        }
      });
      networkStates = builtinNetworkStates;
      this.thirdPartyVpns_ = thirdPartyVpns;
      this.arcVpns_ = arcVpns;
    }

    this.networkStateList_ = networkStates;
  },

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @private
   */
  onNotificationsDisabledDeviceNamesReceived_: function(
      notificationsDisabledDeviceNames) {
    this.notificationsDisabledDeviceNames_ = notificationsDisabledDeviceNames;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return !!deviceState &&
        deviceState.deviceState ==
        chromeos.networkConfig.mojom.DeviceStateType.kEnabled;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOffOnString_: function(deviceState, onstr, offstr) {
    return this.deviceIsEnabled_(deviceState) ? onstr : offstr;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_: function(deviceState) {
    return !!deviceState && deviceState.type != mojom.NetworkType.kEthernet &&
        deviceState.type != mojom.NetworkType.kVPN;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_: function(deviceState) {
    return !!deviceState &&
        deviceState.deviceState !=
        chromeos.networkConfig.mojom.DeviceStateType.kProhibited;
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
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} vpnState
   * @return {string}
   * @private
   */
  getAddThirdPartyVpnA11yString_: function(vpnState) {
    return this.i18n('internetAddThirdPartyVPN', vpnState.ProviderName || '');
  },

  /**
   * @param {!settings.ArcVpnProvider} arcVpn
   * @return {string}
   * @private
   */
  getAddArcVpnAllyString_: function(arcVpn) {
    return this.i18n('internetAddArcVPNProvider', arcVpn.ProviderName);
  },

  /**
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  allowAddConnection_: function(globalPolicy) {
    return globalPolicy && !globalPolicy.AllowOnlyPolicyNetworksToConnect;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  showAddButton_: function(deviceState, globalPolicy) {
    if (!deviceState || deviceState.type != mojom.NetworkType.kWiFi) {
      return false;
    }
    if (!this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    return this.allowAddConnection_(globalPolicy);
  },

  /** @private */
  onAddButtonTap_: function() {
    assert(this.deviceState);
    const type = this.deviceState.type;
    assert(type != mojom.NetworkType.kCellular);
    this.fire('show-config', {type: OncMojo.getNetworkTypeString(type)});
  },

  /**
   * @param {!{model: !{item:
   *     !chrome.networkingPrivate.ThirdPartyVPNProperties}}} event
   * @private
   */
  onAddThirdPartyVpnTap_: function(event) {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.ExtensionID);
  },

  /**
   * @param {!{model: !{item: !settings.ArcVpnProvider}}} event
   * @private
   */
  onAddArcVpnTap_: function(event) {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.AppID);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  knownNetworksIsVisible_: function(deviceState) {
    return !!deviceState && deviceState.type == mojom.NetworkType.kWiFi;
  },

  /**
   * Event triggered when the known networks button is clicked.
   * @private
   */
  onKnownNetworksTap_: function() {
    assert(this.deviceState.type == mojom.NetworkType.kWiFi);
    this.fire('show-known-networks', this.deviceState.type);
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_: function(event) {
    assert(this.deviceState);
    this.fire('device-enabled-toggled', {
      enabled: !this.deviceIsEnabled_(this.deviceState),
      type: this.deviceState.type
    });
  },

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} vpnState
   * @return {!Array<!OncMojo.NetworkStateProperties>}
   * @private
   */
  getThirdPartyVpnNetworks_: function(thirdPartyVpns, vpnState) {
    return thirdPartyVpns[vpnState.ProviderName] || [];
  },

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} vpnState
   * @return {boolean}
   * @private
   */
  haveThirdPartyVpnNetwork_: function(thirdPartyVpns, vpnState) {
    const list = this.getThirdPartyVpnNetworks_(thirdPartyVpns, vpnState);
    return !!list.length;
  },

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} arcVpns
   * @param {!settings.ArcVpnProvider} arcVpnProvider
   * @return {!Array<!OncMojo.NetworkStateProperties>}
   * @private
   */
  getArcVpnNetworks_: function(arcVpns, arcVpnProvider) {
    return arcVpns[arcVpnProvider.PackageName] || [];
  },

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} arcVpns
   * @param {!settings.ArcVpnProvider} arcVpnProvider
   * @return {boolean}
   * @private
   */
  haveArcVpnNetwork_: function(arcVpns, arcVpnProvider) {
    const list = this.getArcVpnNetworks_(arcVpns, arcVpnProvider);
    return !!list.length;
  },

  /**
   * Event triggered when a network list item is selected.
   * @param {!{target: HTMLElement, detail: !OncMojo.NetworkStateProperties}} e
   * @private
   */
  onNetworkSelected_: function(e) {
    assert(this.globalPolicy);
    assert(this.defaultNetwork !== undefined);
    const networkState = e.detail;
    e.target.blur();
    if (this.canConnect_(networkState)) {
      this.fire('network-connect', {networkState: networkState});
      return;
    }
    this.fire('show-detail', networkState);
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} state The network state.
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_: function(state) {
    if (state.type != mojom.NetworkType.kWiFi ||
        this.isPolicySourceMojo(state.source) || !this.globalPolicy) {
      return false;
    }
    return !!this.globalPolicy.AllowOnlyPolicyNetworksToConnect ||
        (!!this.globalPolicy.AllowOnlyPolicyNetworksToConnectIfAvailable &&
         !!this.deviceState && !!this.deviceState.managedNetworkAvailable) ||
        (!!this.globalPolicy.BlacklistedHexSSIDs &&
         this.globalPolicy.BlacklistedHexSSIDs.includes(state.wifi.hexSsid));
  },

  /**
   * Determines whether or not a network state can be connected to.
   * @param {!OncMojo.NetworkStateProperties} state The network state.
   * @private
   */
  canConnect_: function(state) {
    if (state.connectionState != mojom.ConnectionStateType.kNotConnected) {
      return false;
    }
    if (this.isBlockedByPolicy_(state)) {
      return false;
    }
    if (state.type == mojom.NetworkType.kVPN &&
        (!this.defaultNetwork ||
         !OncMojo.connectionStateIsConnected(
             this.defaultNetwork.connectionState))) {
      return false;
    }
    return true;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {boolean}
   * @private
   */
  tetherToggleIsVisible_: function(deviceState, tetherDeviceState) {
    return !!deviceState && deviceState.type == mojom.NetworkType.kCellular &&
        !!tetherDeviceState;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {boolean}
   * @private
   */
  tetherToggleIsEnabled_: function(deviceState, tetherDeviceState) {
    return this.tetherToggleIsVisible_(deviceState, tetherDeviceState) &&
        this.enableToggleIsEnabled_(tetherDeviceState) &&
        tetherDeviceState.deviceState !=
        chromeos.networkConfig.mojom.DeviceStateType.kUninitialized;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onTetherEnabledChange_: function(event) {
    this.fire('device-enabled-toggled', {
      enabled: !this.deviceIsEnabled_(this.tetherDeviceState),
      type: mojom.NetworkType.kTether,
    });
    event.stopPropagation();
  },

  /**
   * @param {string} typeString
   * @param {OncMojo.DeviceStateProperties} device
   * @return {boolean}
   * @private
   */
  matchesType_: function(typeString, device) {
    return device &&
        device.type == OncMojo.getNetworkTypeFromString(typeString);
  },

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowNetworkList_: function(networkStateList) {
    return networkStateList.length > 0;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {string}
   * @private
   */
  getNoNetworksString_: function(deviceState, tetherDeviceState) {
    const type = deviceState.type;
    if (type == mojom.NetworkType.kTether ||
        (type == mojom.NetworkType.kCellular && this.tetherDeviceState)) {
      return this.i18nAdvanced('internetNoNetworksMobileData');
    }

    return this.i18n('internetNoNetworks');
  },

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @return {boolean}
   * @private
   */
  showGmsCoreNotificationsSection_: function(notificationsDisabledDeviceNames) {
    return notificationsDisabledDeviceNames.length > 0;
  },

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @return {string}
   * @private
   */
  getGmsCoreNotificationsDevicesString_: function(
      notificationsDisabledDeviceNames) {
    if (notificationsDisabledDeviceNames.length == 1) {
      return this.i18n(
          'gmscoreNotificationsOneDeviceSubtitle',
          notificationsDisabledDeviceNames[0]);
    }

    if (notificationsDisabledDeviceNames.length == 2) {
      return this.i18n(
          'gmscoreNotificationsTwoDevicesSubtitle',
          notificationsDisabledDeviceNames[0],
          notificationsDisabledDeviceNames[1]);
    }

    return this.i18n('gmscoreNotificationsManyDevicesSubtitle');
  },
});
})();
