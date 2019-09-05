// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

const mojom = chromeos.networkConfig.mojom;

/**
 * @fileoverview
 * 'settings-internet-page' is the settings page containing internet
 * settings.
 */
Polymer({
  is: 'settings-internet-page',

  behaviors: [
    CrNetworkListenerBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Interface for networkingPrivate calls. May be overriden by tests.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
      value: chrome.networkingPrivate,
    },

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The device state for each network device type, keyed by NetworkType. Set
     * by network-summary.
     * @type {!Object<!OncMojo.DeviceStateProperties>|undefined}
     * @private
     */
    deviceStates: {
      type: Object,
      notify: true,
      observer: 'onDeviceStatesChanged_',
    },

    /**
     * Highest priority connected network or null. Set by network-summary.
     * @type {?OncMojo.NetworkStateProperties|undefined}
     */
    defaultNetwork: {
      type: Object,
      notify: true,
    },

    /**
     * Set by internet-subpage. Controls spinner visibility in subpage header.
     * @private
     */
    showSpinner_: Boolean,

    /**
     * The network type for the networks subpage. Used in the subpage header.
     * @private
     */
    subpageType_: String,

    /**
     * The network type for the known networks subpage.
     * @private
     */
    knownNetworksType_: String,

    /**
     * Whether the 'Add connection' section is expanded.
     * @private
     */
    addConnectionExpanded_: {
      type: Boolean,
      value: false,
    },

    /** @private {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} */
    globalPolicy_: Object,

    /**
     * Whether a managed network is available in the visible network list.
     * @private {boolean}
     */
    managedNetworkAvailable: {
      type: Boolean,
      value: false,
    },

    /**
     * List of third party (Extension + Arc) VPN providers.
     * @type {!Array<!chromeos.networkConfig.mojom.VpnProvider>}
     * @private
     */
    vpnProviders_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /** @private {!Map<string, Element>} */
    focusConfig_: {
      type: Object,
      value: function() {
        return new Map();
      },
    },
  },

  /** @private {string} Type of last detail page visited. */
  detailType_: '',

  // Element event listeners
  listeners: {
    'device-enabled-toggled': 'onDeviceEnabledToggled_',
    'network-connect': 'onNetworkConnect_',
    'show-config': 'onShowConfig_',
    'show-detail': 'onShowDetail_',
    'show-known-networks': 'onShowKnownNetworks_',
    'show-networks': 'onShowNetworks_',
  },

  /** @private  {?settings.InternetPageBrowserProxy} */
  browserProxy_: null,

  /**
   * This UI will use both the networkingPrivate extension API and the
   * networkConfig mojo API until we provide all of the required functionality
   * in networkConfig. TODO(stevenjb): Remove use of networkingPrivate api.
   * @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote}
   */
  networkConfig_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.InternetPageBrowserProxyImpl.getInstance();
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /** @override */
  attached: function() {
    this.networkConfig_.getGlobalPolicy().then(response => {
      this.globalPolicy_ = response.result;
    });
    this.onVpnProvidersChanged();
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(route, oldRoute) {
    if (route == settings.routes.INTERNET_NETWORKS) {
      // Handle direct navigation to the networks page,
      // e.g. chrome://settings/internet/networks?type=WiFi
      const queryParams = settings.getQueryParameters();
      const type = queryParams.get('type');
      if (type) {
        this.subpageType_ = type;
      }
    } else if (route == settings.routes.KNOWN_NETWORKS) {
      // Handle direct navigation to the known networks page,
      // e.g. chrome://settings/internet/knownNetworks?type=WiFi
      const queryParams = settings.getQueryParameters();
      const type = queryParams.get('type');
      if (type) {
        this.knownNetworksType_ = type;
      }
    } else if (
        route != settings.routes.INTERNET && route != settings.routes.BASIC) {
      // If we are navigating to a non internet section, do not set focus.
      return;
    }

    if (!settings.routes.INTERNET ||
        !settings.routes.INTERNET.contains(oldRoute)) {
      return;
    }

    // Focus the subpage arrow where appropriate.
    let element;
    if (route == settings.routes.INTERNET_NETWORKS) {
      // iron-list makes the correct timing to focus an item in the list
      // very complicated, and the item may not exist, so just focus the
      // entire list for now.
      const subPage = this.$$('settings-internet-subpage');
      if (subPage) {
        element = subPage.$$('#networkList');
      }
    } else if (this.detailType_) {
      const rowForDetailType =
          this.$$('network-summary').$$(`#${this.detailType_}`);

      // Note: It is possible that the row is no longer present in the DOM
      // (e.g., when a Cellular dongle is unplugged or when Instant Tethering
      // becomes unavailable due to the Bluetooth controller disconnecting).
      if (rowForDetailType) {
        element = rowForDetailType.$$('.subpage-arrow');
      }
    }
    if (element) {
      this.focusConfig_.set(oldRoute.path, element);
    } else {
      this.focusConfig_.delete(oldRoute.path);
    }
  },

  /** CrosNetworkConfigObserver impl */
  onVpnProvidersChanged: function() {
    this.networkConfig_.getVpnProviders().then(response => {
      const providers = response.providers;
      providers.sort(this.compareVpnProviders_);
      this.vpnProviders_ = providers;
    });
  },

  /**
   * Event triggered by a device state enabled toggle.
   * @param {!CustomEvent<!{
   *     enabled: boolean,
   *     type: chromeos.networkConfig.mojom.NetworkType
   * }>} event
   * @private
   */
  onDeviceEnabledToggled_: function(event) {
    this.networkConfig_.setNetworkTypeEnabledState(
        event.detail.type, event.detail.enabled);
  },

  /**
   * @param {!CustomEvent<!{type: string, guid: ?string, name: ?string}>} event
   * @private
   */
  onShowConfig_: function(event) {
    if (!event.detail.guid) {
      // New configuration
      this.showConfig_(true /* configAndConnect */, event.detail.type);
    } else {
      this.showConfig_(
          false /* configAndConnect */, event.detail.type, event.detail.guid,
          event.detail.name);
    }
  },

  /**
   * @param {boolean} configAndConnect
   * @param {string} type
   * @param {?string=} opt_guid
   * @param {?string=} opt_name
   * @private
   */
  showConfig_: function(configAndConnect, type, opt_guid, opt_name) {
    assert(type != CrOnc.Type.CELLULAR && type != CrOnc.Type.TETHER);
    const configDialog =
        /** @type {!InternetConfigElement} */ (this.$.configDialog);
    configDialog.type =
        /** @type {chrome.networkingPrivate.NetworkType} */ (type);
    configDialog.guid = opt_guid || '';
    configDialog.name = opt_name || '';
    configDialog.showConnect = configAndConnect;
    configDialog.open();
  },

  /**
   * @param {!CustomEvent<!OncMojo.NetworkStateProperties>} event
   * @private
   */
  onShowDetail_: function(event) {
    const networkState = event.detail;
    const oncType = OncMojo.getNetworkTypeString(networkState.type);
    this.detailType_ = oncType;
    const params = new URLSearchParams;
    params.append('guid', networkState.guid);
    params.append('type', oncType);
    params.append('name', OncMojo.getNetworkStateDisplayName(networkState));
    settings.navigateTo(settings.routes.NETWORK_DETAIL, params);
  },

  /**
   * @param {!CustomEvent<chromeos.networkConfig.mojom.NetworkType>} event
   * @private
   */
  onShowNetworks_: function(event) {
    this.showNetworksSubpage_(event.detail);
  },

  /**
   * @return {string}
   * @private
   */
  getNetworksPageTitle_: function() {
    // The shared Cellular/Tether subpage is referred to as "Mobile".
    // TODO(khorimoto): Remove once Cellular/Tether are split into their own
    // sections.
    if (this.subpageType_ == CrOnc.Type.CELLULAR ||
        this.subpageType_ == CrOnc.Type.TETHER) {
      return this.i18n('OncTypeMobile');
    }
    return this.i18n('OncType' + this.subpageType_);
  },

  /**
   * @param {string} type
   * @return {string}
   * @private
   */
  getAddNetworkClass_: function(type) {
    return type == CrOnc.Type.WI_FI ? 'icon-add-wifi' : 'icon-add-circle';
  },

  /**
   * @param {string} subpageType
   * @param {!Object<!OncMojo.DeviceStateProperties>|undefined} deviceStates
   * @return {!OncMojo.DeviceStateProperties|undefined}
   * @private
   */
  getDeviceState_: function(subpageType, deviceStates) {
    if (!subpageType) {
      return undefined;
    }
    // If both Tether and Cellular are enabled, use the Cellular device state
    // when directly navigating to the Tether page.
    if (subpageType == CrOnc.Type.TETHER &&
        this.deviceStates[mojom.NetworkType.kCellular]) {
      subpageType = CrOnc.Type.CELLULAR;
    }
    return deviceStates[OncMojo.getNetworkTypeFromString(subpageType)];
  },

  /**
   * @param {!Object<!OncMojo.DeviceStateProperties>|undefined} deviceStates
   * @return {!OncMojo.DeviceStateProperties|undefined}
   * @private
   */
  getTetherDeviceState_: function(deviceStates) {
    return deviceStates[mojom.NetworkType.kTether];
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} newValue
   * @param {!OncMojo.DeviceStateProperties|undefined} oldValue
   * @private
   */
  onDeviceStatesChanged_: function(newValue, oldValue) {
    const wifiDeviceState = this.getDeviceState_(CrOnc.Type.WI_FI, newValue);
    let managedNetworkAvailable = false;
    if (wifiDeviceState) {
      managedNetworkAvailable = !!wifiDeviceState.managedNetworkAvailable;
    }

    if (this.managedNetworkAvailable != managedNetworkAvailable) {
      this.managedNetworkAvailable = managedNetworkAvailable;
    }

    if (this.detailType_ &&
        !this.deviceStates[OncMojo.getNetworkTypeFromString(
            this.detailType_)]) {
      // If the device type associated with the current network has been
      // removed (e.g., due to unplugging a Cellular dongle), the details page,
      // if visible, displays controls which are no longer functional. If this
      // case occurs, close the details page.
      const detailPage = this.$$('settings-internet-detail-page');
      if (detailPage) {
        detailPage.close();
      }
    }
  },

  /**
   * @param {!CustomEvent<chromeos.networkConfig.mojom.NetworkType>} event
   * @private
   */
  onShowKnownNetworks_: function(event) {
    const oncType = OncMojo.getNetworkTypeString(event.detail);
    this.detailType_ = oncType;
    this.knownNetworksType_ = oncType;
    const params = new URLSearchParams;
    params.append('type', oncType);
    settings.navigateTo(settings.routes.KNOWN_NETWORKS, params);
  },

  /** @private */
  onAddWiFiTap_: function() {
    this.showConfig_(true /* configAndConnect */, CrOnc.Type.WI_FI);
  },

  /** @private */
  onAddVPNTap_: function() {
    this.showConfig_(true /* configAndConnect */, CrOnc.Type.VPN);
  },

  /**
   * @param {!{model: !{item: !mojom.VpnProvider}}} event
   * @private
   */
  onAddThirdPartyVpnTap_: function(event) {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.appId);
  },

  /**
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @private
   */
  showNetworksSubpage_: function(type) {
    const oncType = OncMojo.getNetworkTypeString(type);
    this.detailType_ = oncType;
    const params = new URLSearchParams;
    params.append('type', oncType);
    this.subpageType_ = oncType;
    settings.navigateTo(settings.routes.INTERNET_NETWORKS, params);
  },

  /**
   * @param {!mojom.VpnProvider} vpnProvider1
   * @param {!mojom.VpnProvider} vpnProvider2
   * @return {number}
   */
  compareVpnProviders_: function(vpnProvider1, vpnProvider2) {
    // Show Extension VPNs before Arc VPNs.
    if (vpnProvider1.type < vpnProvider2.type) {
      return -1;
    }
    if (vpnProvider1.type > vpnProvider2.type) {
      return 1;
    }
    // Show VPNs of the same type by lastLaunchTime.
    if (vpnProvider1.lastLaunchTime.internalValue >
        vpnProvider2.lastLaunchTime.internalValue) {
      return -1;
    }
    if (vpnProvider1.lastLaunchTime.internalValue <
        vpnProvider2.lastLaunchTime.internalValue) {
      return 1;
    }
    return 0;
  },

  /**
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStates
   * @param {string} type
   * @return {boolean}
   * @private
   */
  deviceIsEnabled_: function(deviceStates, type) {
    const device = deviceStates[OncMojo.getNetworkTypeFromString(type)];
    return !!device &&
        device.deviceState ==
        chromeos.networkConfig.mojom.DeviceStateType.kEnabled;
  },

  /**
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   */
  allowAddConnection_: function(globalPolicy, managedNetworkAvailable) {
    if (!globalPolicy) {
      return true;
    }

    return !globalPolicy.allowOnlyPolicyNetworksToConnect &&
        (!globalPolicy.allowOnlyPolicyNetworksToConnectIfAvailable ||
         !managedNetworkAvailable);
  },

  /**
   * @param {!mojom.VpnProvider} provider
   * @return {string}
   */
  getAddThirdPartyVpnLabel_: function(provider) {
    return this.i18n('internetAddThirdPartyVPN', provider.providerName || '');
  },

  /**
   * Handles UI requests to connect to a network.
   * TODO(stevenjb): Handle Cellular activation, etc.
   * @param {!CustomEvent<!{
   *     networkState: !OncMojo.NetworkStateProperties,
   *     bypassConnectionDialog: (boolean|undefined)
   * }>} event
   * @private
   */
  onNetworkConnect_: function(event) {
    const networkState = event.detail.networkState;
    const oncType = OncMojo.getNetworkTypeString(networkState.type);
    const displayName = OncMojo.getNetworkStateDisplayName(networkState);

    if (!event.detail.bypassConnectionDialog &&
        networkState.type == mojom.NetworkType.kTether &&
        !networkState.tether.hasConnectedToHost) {
      const params = new URLSearchParams;
      params.append('guid', networkState.guid);
      params.append('type', oncType);
      params.append('name', displayName);
      params.append('showConfigure', true.toString());

      settings.navigateTo(settings.routes.NETWORK_DETAIL, params);
      return;
    }

    const isMobile = OncMojo.networkTypeIsMobile(networkState.type);
    if (!isMobile && (!networkState.connectable || !!networkState.errorState)) {
      this.showConfig_(
          true /* configAndConnect */, oncType, networkState.guid, displayName);
      return;
    }

    this.networkConfig_.startConnect(networkState.guid).then(response => {
      switch (response.result) {
        case mojom.StartConnectResult.kSuccess:
          return;
        case mojom.StartConnectResult.kInvalidGuid:
        case mojom.StartConnectResult.kInvalidState:
        case mojom.StartConnectResult.kCanceled:
          // TODO(stevenjb/khorimoto): Consider handling these cases.
          return;
        case mojom.StartConnectResult.kNotConfigured:
          if (!isMobile) {
            this.showConfig_(
                true /* configAndConnect */, oncType, networkState.guid,
                displayName);
          }
          return;
        case mojom.StartConnectResult.kBlocked:
          // This shouldn't happen, the UI should prevent this, fall through and
          // show the error.
        case mojom.StartConnectResult.kUnknown:
          console.error(
              'startConnect failed for: ' + networkState.guid +
              ' Error: ' + response.message);
          return;
      }
      assertNotReached();
    });
  },
});
})();
