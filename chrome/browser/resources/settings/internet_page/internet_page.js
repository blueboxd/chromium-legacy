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

    /** @private {!chrome.networkingPrivate.GlobalPolicy|undefined} */
    globalPolicy_: {
      type: Object,
      value: null,
    },

    /**
     * Whether a managed network is available in the visible network list.
     * @private {boolean}
     */
    managedNetworkAvailable: {
      type: Boolean,
      value: false,
    },

    /**
     * List of third party VPN providers.
     * @type {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>}
     * @private
     */
    thirdPartyVpnProviders_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * List of Arc VPN providers.
     * @type {!Array<!settings.ArcVpnProvider>}
     * @private
     */
    arcVpnProviders_: {
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

  // chrome.management listeners
  /** @private {Function} */
  onExtensionAddedListener_: null,

  /** @private {Function} */
  onExtensionRemovedListener_: null,

  /** @private {Function} */
  onExtensionDisabledListener_: null,

  /** @private  {?settings.InternetPageBrowserProxy} */
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
    this.browserProxy_.setUpdateArcVpnProvidersCallback(
        this.onArcVpnProvidersReceived_.bind(this));
    this.browserProxy_.requestArcVpnProviders();
  },

  /** @override */
  attached: function() {
    this.onExtensionAddedListener_ =
        this.onExtensionAddedListener_ || this.onExtensionAdded_.bind(this);
    chrome.management.onInstalled.addListener(this.onExtensionAddedListener_);
    chrome.management.onEnabled.addListener(this.onExtensionAddedListener_);

    this.onExtensionRemovedListener_ =
        this.onExtensionRemovedListener_ || this.onExtensionRemoved_.bind(this);
    chrome.management.onUninstalled.addListener(
        this.onExtensionRemovedListener_);

    this.onExtensionDisabledListener_ = this.onExtensionDisabledListener_ ||
        this.onExtensionDisabled_.bind(this);
    chrome.management.onDisabled.addListener(this.onExtensionDisabledListener_);

    chrome.management.getAll(this.onGetAllExtensions_.bind(this));

    this.networkingPrivate.getGlobalPolicy(policy => {
      this.globalPolicy_ = policy;
    });
  },

  /** @override */
  detached: function() {
    chrome.management.onInstalled.removeListener(
        assert(this.onExtensionAddedListener_));
    chrome.management.onEnabled.removeListener(
        assert(this.onExtensionAddedListener_));
    chrome.management.onUninstalled.removeListener(
        assert(this.onExtensionRemovedListener_));
    chrome.management.onDisabled.removeListener(
        assert(this.onExtensionDisabledListener_));
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

  /**
   * Event triggered by a device state enabled toggle.
   * @param {!CustomEvent<!{
   *     enabled: boolean,
   *     type: chromeos.networkConfig.mojom.NetworkType
   * }>} event
   * @private
   */
  onDeviceEnabledToggled_: function(event) {
    this.networkConfigProxy_.setNetworkTypeEnabledState(
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
    params.append('name', OncMojo.getNetworkDisplayName(networkState));
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
   * @param {!{model:
   *            !{item: !chrome.networkingPrivate.ThirdPartyVPNProperties},
   *        }} event
   * @private
   */
  onAddThirdPartyVpnTap_: function(event) {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.ExtensionID);
  },

  /** @private */
  onAddArcVpnTap_: function() {
    this.showNetworksSubpage_(mojom.NetworkType.kVPN);
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
   * chrome.management.getAll callback.
   * @param {!Array<!chrome.management.ExtensionInfo>} extensions
   * @private
   */
  onGetAllExtensions_: function(extensions) {
    const vpnProviders = [];
    for (let i = 0; i < extensions.length; ++i) {
      this.addVpnProvider_(vpnProviders, extensions[i]);
    }
    this.thirdPartyVpnProviders_ = vpnProviders;
  },

  /**
   * If |extension| is a third-party VPN provider, add it to |vpnProviders|.
   * @param {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>}
   *     vpnProviders
   * @param {!chrome.management.ExtensionInfo} extension
   * @private
   */
  addVpnProvider_: function(vpnProviders, extension) {
    if (!extension.enabled ||
        extension.permissions.indexOf('vpnProvider') == -1) {
      return;
    }
    if (vpnProviders.find(function(provider) {
          return provider.ExtensionID == extension.id;
        })) {
      return;
    }
    const newProvider = {
      ExtensionID: extension.id,
      ProviderName: extension.name,
    };
    vpnProviders.push(newProvider);
  },

  /**
   * chrome.management.onInstalled or onEnabled event.
   * @param {!chrome.management.ExtensionInfo} extension
   * @private
   */
  onExtensionAdded_: function(extension) {
    this.addVpnProvider_(this.thirdPartyVpnProviders_, extension);
  },

  /**
   * chrome.management.onUninstalled event.
   * @param {string} extensionId
   * @private
   */
  onExtensionRemoved_: function(extensionId) {
    for (let i = 0; i < this.thirdPartyVpnProviders_.length; ++i) {
      const provider = this.thirdPartyVpnProviders_[i];
      if (provider.ExtensionID == extensionId) {
        this.splice('thirdPartyVpnProviders_', i, 1);
        break;
      }
    }
  },

  /**
   * Compares Arc VPN Providers based on LastlauchTime
   * @param {!settings.ArcVpnProvider} arcVpnProvider1
   * @param {!settings.ArcVpnProvider} arcVpnProvider2
   * @private
   */
  compareArcVpnProviders_: function(arcVpnProvider1, arcVpnProvider2) {
    if (arcVpnProvider1.LastLaunchTime > arcVpnProvider2.LastLaunchTime) {
      return -1;
    }
    if (arcVpnProvider1.LastLaunchTime < arcVpnProvider2.LastLaunchTime) {
      return 1;
    }
    return 0;
  },

  /**
   * @param {?Array<!settings.ArcVpnProvider>} arcVpnProviders
   * @private
   */
  onArcVpnProvidersReceived_: function(arcVpnProviders) {
    arcVpnProviders.sort(this.compareArcVpnProviders_);
    this.arcVpnProviders_ = arcVpnProviders;
  },

  /**
   * chrome.management.onDisabled event.
   * @param {{id: string}} extension
   * @private
   */
  onExtensionDisabled_: function(extension) {
    this.onExtensionRemoved_(extension.id);
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
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   */
  allowAddConnection_: function(globalPolicy, managedNetworkAvailable) {
    if (!globalPolicy) {
      return true;
    }

    return !globalPolicy.AllowOnlyPolicyNetworksToConnect &&
        (!globalPolicy.AllowOnlyPolicyNetworksToConnectIfAvailable ||
         !managedNetworkAvailable);
  },

  /**
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} provider
   * @return {string}
   */
  getAddThirdPartyVpnLabel_: function(provider) {
    return this.i18n('internetAddThirdPartyVPN', provider.ProviderName || '');
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
    const displayName = OncMojo.getNetworkDisplayName(networkState);

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

    this.networkingPrivate.startConnect(networkState.guid, () => {
      if (chrome.runtime.lastError) {
        const message = chrome.runtime.lastError.message;
        if (message == 'connecting' || message == 'connect-canceled' ||
            message == 'connected' || message == 'Error.InvalidNetworkGuid') {
          return;
        }
        console.error(
            'networkingPrivate.startConnect error: ' + message +
            ' For: ' + networkState.guid);

        // There is no configuration flow for Mobile Networks.
        if (!isMobile) {
          this.showConfig_(
              true /* configAndConnect */, oncType, networkState.guid,
              displayName);
        }
      }
    });
  },
});
})();
