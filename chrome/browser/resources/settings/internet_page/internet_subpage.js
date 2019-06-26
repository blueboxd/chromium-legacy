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
     * @type {?CrOnc.NetworkStateProperties|undefined}
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

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {!NetworkingPrivate}
     */
    networkingPrivate: Object,

    showSpinner: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /**
     * List of all network state data for the network type.
     * @private {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Dictionary of lists of network states for third party VPNs.
     * @private {!Object<!Array<!CrOnc.NetworkStateProperties>>}
     */
    thirdPartyVpns_: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /**
     * Dictionary of lists of network states for Arc VPNs.
     * @private {!Object<!Array<!CrOnc.NetworkStateProperties>>}
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
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
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
    const type = /** @type{CrOnc.Type} */ (
        OncMojo.getNetworkTypeString(this.deviceState.type));
    const filter = {networkType: type, visible: true, configured: false};
    this.networkingPrivate.getNetworks(filter, this.onGetNetworks_.bind(this));
  },

  /**
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStates
   * @private
   */
  onGetNetworks_: function(networkStates) {
    if (!this.deviceState) {
      return;
    }  // Edge case when device states change before this callback.

    // For the Cellular/Mobile subpage, request Tether networks if available.
    if (this.deviceState.type == mojom.NetworkType.kCellular &&
        this.tetherDeviceState) {
      const filter = {
        networkType: CrOnc.Type.TETHER,
        visible: true,
        configured: false
      };
      this.networkingPrivate.getNetworks(filter, tetherNetworkStates => {
        this.networkStateList_ = networkStates.concat(tetherNetworkStates);
      });
      return;
    }

    // For VPNs, separate out third party VPNs and Arc VPNs.
    if (this.deviceState.type == mojom.NetworkType.kVPN) {
      const builtinNetworkStates = [];
      const thirdPartyVpns = {};
      const arcVpns = {};
      for (let i = 0; i < networkStates.length; ++i) {
        const state = networkStates[i];
        const providerType = this.get('VPN.ThirdPartyVPN.ProviderName', state);
        if (providerType) {
          thirdPartyVpns[providerType] = thirdPartyVpns[providerType] || [];
          thirdPartyVpns[providerType].push(state);
        } else if (this.get('VPN.Type', state) == 'ARCVPN') {
          const arcProviderName = this.get('VPN.Host', state);
          if (state.ConnectionState != CrOnc.ConnectionState.CONNECTED) {
            continue;
          }
          arcVpns[arcProviderName] = arcVpns[arcProviderName] || [];
          arcVpns[arcProviderName].push(state);
        } else {
          builtinNetworkStates.push(state);
        }
      }
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
    const type = OncMojo.getNetworkTypeString(this.deviceState.type);
    assert(type != CrOnc.Type.CELLULAR);
    this.fire('show-config', {GUID: '', Type: type});
  },

  /**
   * @param {!{model:
   *              !{item: !chrome.networkingPrivate.ThirdPartyVPNProperties},
   *        }} event
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
   * @param {!Object<!Array<!CrOnc.NetworkStateProperties>>} thirdPartyVpns
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} vpnState
   * @return {!Array<!CrOnc.NetworkStateProperties>}
   * @private
   */
  getThirdPartyVpnNetworks_: function(thirdPartyVpns, vpnState) {
    return thirdPartyVpns[vpnState.ProviderName] || [];
  },

  /**
   * @param {!Object<!Array<!CrOnc.NetworkStateProperties>>} thirdPartyVpns
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} vpnState
   * @return {boolean}
   * @private
   */
  haveThirdPartyVpnNetwork_: function(thirdPartyVpns, vpnState) {
    const list = this.getThirdPartyVpnNetworks_(thirdPartyVpns, vpnState);
    return !!list.length;
  },

  /**
   * @param {!Object<!Array<!CrOnc.NetworkStateProperties>>} arcVpns
   * @param {!settings.ArcVpnProvider} arcVpnProvider
   * @return {!Array<!CrOnc.NetworkStateProperties>}
   * @private
   */
  getArcVpnNetworks_: function(arcVpns, arcVpnProvider) {
    return arcVpns[arcVpnProvider.PackageName] || [];
  },

  /**
   * @param {!Object<!Array<!CrOnc.NetworkStateProperties>>} arcVpns
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
   * @param {!{target: HTMLElement, detail: !CrOnc.NetworkStateProperties}} e
   * @private
   */
  onNetworkSelected_: function(e) {
    assert(this.globalPolicy);
    assert(this.defaultNetwork !== undefined);
    const state = e.detail;
    e.target.blur();
    if (this.canConnect_(state)) {
      this.fire('network-connect', {networkProperties: state});
      return;
    }
    this.fire('show-detail', state);
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} state The network state.
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_: function(state) {
    if (state.Type != CrOnc.Type.WI_FI || this.isPolicySource(state.Source) ||
        !this.globalPolicy) {
      return false;
    }
    return !!this.globalPolicy.AllowOnlyPolicyNetworksToConnect ||
        (!!this.globalPolicy.AllowOnlyPolicyNetworksToConnectIfAvailable &&
         !!this.deviceState && !!this.deviceState.managedNetworkAvailable) ||
        (!!state.WiFi && !!state.WiFi.HexSSID &&
         !!this.globalPolicy.BlacklistedHexSSIDs &&
         this.globalPolicy.BlacklistedHexSSIDs.includes(state.WiFi.HexSSID));
  },

  /**
   * Determines whether or not a network state can be connected to.
   * @param {!CrOnc.NetworkStateProperties} state The network state.
   * @private
   */
  canConnect_: function(state) {
    if (state.ConnectionState != CrOnc.ConnectionState.NOT_CONNECTED) {
      return false;
    }
    if (this.isBlockedByPolicy_(state)) {
      return false;
    }
    if (state.Type == CrOnc.Type.VPN &&
        (!this.defaultNetwork ||
         this.defaultNetwork.ConnectionState !=
             CrOnc.ConnectionState.CONNECTED)) {
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
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
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
