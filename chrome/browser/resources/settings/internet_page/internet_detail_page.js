// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-detail' is the settings subpage containing details
 * for a network.
 */
(function() {
'use strict';

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'settings-internet-detail-page',

  behaviors: [
    CrNetworkListenerBehavior,
    CrPolicyNetworkBehaviorMojo,
    settings.RouteObserverBehavior,
    I18nBehavior,
  ],

  properties: {
    /** The network GUID to display details for. */
    guid: String,

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current properties for the network matching |guid|. Note: This may
     * become set to |undefined| after it is initially set if the network is no
     * longer visible, so always test that it is set before accessing it.
     * @private {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties_: {
      type: Object,
    },

    /** @private {!OncMojo.ManagedProperties|undefined} */
    managedProperties_: {
      type: Object,
      observer: 'managedPropertiesChanged_',
    },

    /** @private {?OncMojo.DeviceStateProperties} */
    deviceState_: {
      type: Object,
      value: null,
    },

    /**
     * Whether the user is a secondary user.
     * @private
     */
    isSecondaryUser_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isSecondaryUser');
      },
      readOnly: true,
    },

    /**
     * Email address for the primary user.
     * @private
     */
    primaryUserEmail_: {
      type: String,
      value: function() {
        return loadTimeData.getBoolean('isSecondaryUser') ?
            loadTimeData.getString('primaryUserEmail') :
            '';
      },
      readOnly: true,
    },

    /**
     * Whether the network has been lost (e.g., has gone out of range). A
     * network is considered to be lost when a OnNetworkStateListChanged
     * is signaled and the new network list does not contain the GUID of the
     * current network.
     * @private
     */
    outOfRange_: {
      type: Boolean,
      value: false,
    },

    /**
     * Highest priority connected network or null.
     * @type {?OncMojo.NetworkStateProperties}
     */
    defaultNetwork: {
      type: Object,
      value: null,
    },

    /** @type {!chrome.networkingPrivate.GlobalPolicy|undefined} */
    globalPolicy: {
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
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * The network AutoConnect state as a fake preference object.
     * @private {!chrome.settingsPrivate.PrefObject|undefined}
     */
    autoConnectPref_: {
      type: Object,
      observer: 'autoConnectPrefChanged_',
    },

    /**
     * The always-on VPN state as a fake preference object.
     * @private {!chrome.settingsPrivate.PrefObject|undefined}
     */
    alwaysOnVpn_: {
      type: Object,
      observer: 'alwaysOnVpnChanged_',
      value: function() {
        return {
          key: 'fakeAlwaysOnPref',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        };
      }
    },

    /**
     * The network preferred state.
     * @private
     */
    preferNetwork_: {
      type: Boolean,
      value: false,
      observer: 'preferNetworkChanged_',
    },

    /**
     * The network IP Address.
     * @private
     */
    ipAddress_: {
      type: String,
      value: '',
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

    /** @private */
    advancedExpanded_: Boolean,

    /** @private */
    networkExpanded_: Boolean,

    /** @private */
    proxyExpanded_: Boolean,
  },

  observers: [
    'updateAlwaysOnVpnPrefValue_(prefs.arc.vpn.always_on.*)',
    'updateAlwaysOnVpnPrefEnforcement_(managedProperties_,' +
        'prefs.vpn_config_allowed.*)',
    'updateAutoConnectPref_(globalPolicy)',
    'autoConnectPrefChanged_(autoConnectPref_.*)',
    'alwaysOnVpnChanged_(alwaysOnVpn_.*)',
  ],

  /** @private {boolean} */
  didSetFocus_: false,

  /**
   * Set to true to once the initial properties have been received. This
   * prevents setProperties from being called when setting default properties.
   * @private {boolean}
   */
  networkPropertiesReceived_: false,

  /**
   * Set in currentRouteChanged() if the showConfigure URL query
   * parameter is set to true. The dialog cannot be shown until the
   * network properties have been fetched in networkPropertiesChanged_().
   * @private {boolean}
   */
  shouldShowConfigureWhenNetworkLoaded_: false,

  /** @private  {settings.InternetPageBrowserProxy} */
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

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(route, oldRoute) {
    if (route != settings.routes.NETWORK_DETAIL) {
      return;
    }

    const queryParams = settings.getQueryParameters();
    const guid = queryParams.get('guid') || '';
    if (!guid) {
      console.error('No guid specified for page:' + route);
      this.close();
    }

    this.shouldShowConfigureWhenNetworkLoaded_ =
        queryParams.get('showConfigure') == 'true';
    const type = /** @type {!chrome.networkingPrivate.NetworkType} */ (
                     queryParams.get('type')) ||
        CrOnc.Type.WI_FI;
    const name = queryParams.get('name') || type;
    this.init(guid, type, name);
  },

  /**
   * @param {string} guid
   * @param {!chrome.networkingPrivate.NetworkType} type
   * @param {string} name
   */
  init: function(guid, type, name) {
    this.guid = guid;
    // Set basic networkProperties until they are loaded.
    this.networkPropertiesReceived_ = false;
    this.networkProperties_ = {
      GUID: this.guid,
      Type: type,
      Name: {Active: name},
    };
    this.managedProperties_ = OncMojo.getDefaultManagedProperties(
        OncMojo.getNetworkTypeFromString(type), this.guid, name);
    this.didSetFocus_ = false;
    this.getNetworkDetails_();
  },

  close: function() {
    // If the page is already closed, return early to avoid navigating backward
    // erroneously.
    if (!this.guid) {
      return;
    }

    this.guid = '';

    // Delay navigating to allow other subpages to load first.
    requestAnimationFrame(() => {
      // Clear network properties before navigating away to ensure that a future
      // navigation back to the details page does not show a flicker of
      // incorrect text. See https://crbug.com/905986.
      this.networkProperties_ = undefined;
      this.managedProperties_ = undefined;
      this.networkPropertiesReceived_ = false;

      settings.navigateToPreviousRoute();
    });
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged: function(networks) {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    // If the network was or is active, request an update.
    if (this.managedProperties_.connectionState !=
            mojom.ConnectionStateType.kNotConnected ||
        networks.find(network => network.guid == this.guid)) {
      this.getNetworkDetails_();
    }
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network
   */
  onNetworkStateChanged: function(network) {
    if (!this.guid || !this.networkProperties_) {
      return;
    }
    if (network.guid == this.guid) {
      this.getNetworkDetails_();
    }
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.checkNetworkExists_();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    this.getDeviceState_();
    if (this.guid) {
      this.getNetworkDetails_();
    }
  },

  /** @private */
  managedPropertiesChanged_: function() {
    if (!this.managedProperties_) {
      return;
    }
    this.updateAutoConnectPref_();

    const priority = this.managedProperties_.priority;
    if (priority) {
      const preferNetwork = priority.activeValue > 0;
      if (preferNetwork != this.preferNetwork_) {
        this.preferNetwork_ = preferNetwork;
      }
    }

    // Set the IPAddress property to the IPv4 Address.
    const ipv4 =
        OncMojo.getIPConfigForType(this.managedProperties_, CrOnc.IPType.IPV4);
    this.ipAddress_ = (ipv4 && ipv4.ipAddress) || '';

    // Update the detail page title.
    const networkName = OncMojo.getNetworkName(this.managedProperties_);
    this.parentNode.pageTitle = networkName;
    Polymer.dom.flush();

    if (!this.didSetFocus_) {
      // Focus a button once the initial state is set.
      this.didSetFocus_ = true;
      const button = this.$$('#titleDiv .action-button:not([hidden])') ||
          this.$$('#titleDiv cr-button:not([hidden])');
      if (button) {
        setTimeout(() => button.focus());
      }
    }

    if (this.shouldShowConfigureWhenNetworkLoaded_ &&
        this.managedProperties_.type == mojom.NetworkType.kTether) {
      // Set |this.shouldShowConfigureWhenNetworkLoaded_| back to false to
      // ensure that the Tether dialog is only shown once.
      this.shouldShowConfigureWhenNetworkLoaded_ = false;
      this.showTetherDialog_();
    }
  },

  /** @private */
  getDeviceState_: function() {
    if (!this.managedProperties_) {
      return;
    }
    this.networkConfig_.getDeviceStateList().then(response => {
      const devices = response.result;
      this.deviceState_ =
          devices.find(device => device.type == this.managedProperties_.type) ||
          null;
    });
  },

  /** @private */
  autoConnectPrefChanged_: function() {
    if (!this.networkProperties_ || !this.guid) {
      return;
    }
    const config = {};
    config.autoConnect = {value: !!this.autoConnectPref_.value};
    this.setMojoNetworkProperties_(config);
  },

  /**
   * Updates auto-connect pref value.
   * @private
   */
  updateAutoConnectPref_: function() {
    if (!this.managedProperties_) {
      return;
    }
    const autoConnect = OncMojo.getManagedAutoConnect(this.managedProperties_);
    if (!autoConnect) {
      return;
    }

    let enforcement;
    let controlledBy;
    if (autoConnect.enforced ||
        (!!this.globalPolicy &&
         !!this.globalPolicy.AllowOnlyPolicyNetworksToAutoconnect)) {
      enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
    }

    if (this.autoConnectPref_ &&
        this.autoConnectPref_.value == autoConnect.activeValue &&
        enforcement == this.autoConnectPref_.enforcement &&
        controlledBy == this.autoConnectPref_.controlledBy) {
      return;
    }

    const newPrefValue = {
      key: 'fakeAutoConnectPref',
      value: autoConnect.activeValue,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    if (enforcement) {
      newPrefValue.enforcement = enforcement;
      newPrefValue.controlledBy = controlledBy;
    }

    this.autoConnectPref_ = newPrefValue;
  },

  /** @private */
  preferNetworkChanged_: function() {
    if (!this.networkProperties_ || !this.guid) {
      return;
    }
    const config = {};
    config.priority = {value: this.preferNetwork_ ? 1 : 0};
    this.setMojoNetworkProperties_(config);
  },

  /** @private */
  checkNetworkExists_: function() {
    const filter = {
      networkType: CrOnc.Type.ALL,
      visible: true,
      configured: false
    };
    this.networkingPrivate.getNetworks(filter, networks => {
      if (networks.find(network => network.GUID == this.guid)) {
        return;
      }
      this.outOfRange_ = true;
      if (this.managedProperties_) {
        // Set the connection state since we won't receive an update for a non
        // existent network.
        this.managedProperties_.connectionState =
            mojom.ConnectionStateType.kNotConnected;
      }
    });
  },

  /**
   * Calls networkingPrivate.getProperties for this.guid.
   * @private
   */
  getNetworkDetails_: function() {
    assert(this.guid);
    if (this.isSecondaryUser_) {
      this.networkConfig_.getNetworkState(this.guid).then(response => {
        this.getStateCallback_(response.result);
      });
    } else {
      this.networkingPrivate.getManagedProperties(
          this.guid, this.getPropertiesCallback_.bind(this));
    }
  },

  /**
   * networkingPrivate.getProperties callback.
   * @param {!CrOnc.NetworkProperties} properties The network properties.
   * @private
   */
  getPropertiesCallback_: function(properties) {
    if (chrome.runtime.lastError) {
      const message = chrome.runtime.lastError.message;
      if (message == 'Error.InvalidNetworkGuid') {
        console.error('Details page: GUID no longer exists: ' + this.guid);
      } else {
        console.error(
            'Unexpected networkingPrivate.getManagedProperties error: ' +
            message + ' For: ' + this.guid);
      }
      this.close();
      return;
    }

    // Details page was closed while request was in progress, ignore the result.
    if (!this.guid) {
      return;
    }

    if (!properties) {
      // Edge case, may occur when disabling. Close this.
      this.close();
      return;
    }

    // Get the managed properties and then update networkProperties_, etc.
    this.networkConfig_.getManagedProperties(this.guid).then(response => {
      if (!response.result) {
        // Edge case, may occur when disabling. Close this.
        this.close();
        return;
      }
      this.managedProperties_ = response.result;
      // Detail page should not be shown when Arc VPN is not connected.
      if (this.isArcVpn_(this.managedProperties_) &&
          !this.isConnectedState_(this.managedProperties_)) {
        this.guid = '';
        this.close();
      }
      this.networkProperties_ = properties;
      this.networkPropertiesReceived_ = true;
      this.outOfRange_ = false;
      if (!this.deviceState_) {
        this.getDeviceState_();
      }
    });
  },

  /**
   * @param {?OncMojo.NetworkStateProperties} networkState
   * @private
   */
  getStateCallback_: function(networkState) {
    if (!networkState) {
      // Edge case, may occur when disabling. Close this.
      this.close();
      return;
    }
    const type = /** @type {CrOnc.Type} */ (
        OncMojo.getNetworkTypeString(networkState.type));

    let connectionState;
    switch (networkState.connectionState) {
      case mojom.ConnectionStateType.kOnline:
      case mojom.ConnectionStateType.kConnected:
      case mojom.ConnectionStateType.kPortal:
        connectionState = CrOnc.ConnectionState.CONNECTED;
        break;
      case mojom.ConnectionStateType.kConnecting:
        connectionState = CrOnc.ConnectionState.CONNECTING;
        break;
      case mojom.ConnectionStateType.kNotConnected:
        connectionState = CrOnc.ConnectionState.NOT_CONNECTED;
        break;
    }

    this.networkProperties_ = {
      GUID: networkState.guid,
      Name: {Active: networkState.name},
      Type: type,
      Connectable: networkState.connectable,
      ConnectionState: connectionState,
    };
    this.managedProperties_ = OncMojo.getDefaultManagedProperties(
        OncMojo.getNetworkTypeFromString(type), networkState.guid,
        networkState.name);
    this.managedProperties_.connectable = networkState.connectable;
    this.managedProperties_.connectionState = networkState.connectionState;

    this.networkPropertiesReceived_ = true;
    this.outOfRange_ = false;
  },

  /**
   * @param {!mojom.ManagedProperties} properties
   * @return {!OncMojo.NetworkStateProperties|undefined}
   */
  getNetworkState_: function(properties) {
    if (!properties) {
      return undefined;
    }
    return OncMojo.managedPropertiesToNetworkState(properties);
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} onc The ONC
   *     network properties.
   * @private
   */
  setNetworkProperties_: function(onc) {
    if (!this.networkPropertiesReceived_ || !this.guid) {
      return;
    }
    this.networkingPrivate.setProperties(this.guid, onc, () => {
      if (chrome.runtime.lastError) {
        // An error typically indicates invalid input; request the properties
        // to update any invalid fields.
        this.getNetworkDetails_();
      }
    });
  },

  /**
   * @param {!mojom.ConfigProperties} config
   * @private
   */
  setMojoNetworkProperties_: function(config) {
    if (!this.networkPropertiesReceived_ || !this.guid) {
      return;
    }
    this.networkConfig_.setProperties(this.guid, config).then(response => {
      if (!response.success) {
        console.error('Unable to set properties: ' + JSON.stringify(config));
        // An error typically indicates invalid input; request the properties
        // to update any invalid fields.
        this.getNetworkDetails_();
      }
    });
  },

  /**
   * @return {!chrome.networkingPrivate.NetworkConfigProperties} An ONC
   *     dictionary with just the Type property set. Used for passing properties
   *     to setNetworkProperties_.
   * @private
   */
  getEmptyNetworkProperties_: function() {
    const type = this.networkProperties_ ? this.networkProperties_.Type :
                                           CrOnc.Type.WI_FI;
    return {Type: type};
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {boolean} outOfRange
   * @return {string} The text to display for the network connection state.
   * @private
   */
  getStateText_: function(managedProperties, outOfRange) {
    if (!managedProperties) {
      return '';
    }

    if (outOfRange) {
      return managedProperties.type == mojom.NetworkType.kTether ?
          this.i18n('tetherPhoneOutOfRange') :
          this.i18n('networkOutOfRange');
    }

    return this.i18n(
        OncMojo.getConnectionStateString(managedProperties.connectionState));
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string} The text to display for auto-connect toggle label.
   * @private
   */
  getAutoConnectToggleLabel_: function(managedProperties) {
    return this.isCellular_(managedProperties) ?
        this.i18n('networkAutoConnectCellular') :
        this.i18n('networkAutoConnect');
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string} The text to display with roaming details.
   * @private
   */
  getRoamingDetails_: function(managedProperties) {
    if (!this.isCellular_(managedProperties)) {
      return '';
    }
    if (!managedProperties.cellular.allowRoaming) {
      return this.i18n('networkAllowDataRoamingDisabled');
    }

    return managedProperties.cellular.roamingState ==
            CrOnc.RoamingState.ROAMING ?
        this.i18n('networkAllowDataRoamingEnabledRoaming') :
        this.i18n('networkAllowDataRoamingEnabledHome');
  },

  /**
   * @param {!OncMojo.ManagedProperties|undefined} managedProperties
   * @return {boolean} True if the network is connected.
   * @private
   */
  isConnectedState_: function(managedProperties) {
    return !!managedProperties &&
        OncMojo.connectionStateIsConnected(managedProperties.connectionState);
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isRemembered_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.source != mojom.OncSource.kNone;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isRememberedOrConnected_: function(managedProperties) {
    return this.isRemembered_(managedProperties) ||
        this.isConnectedState_(managedProperties);
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isCellular_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kCellular;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties || !globalPolicy ||
        managedProperties.type != mojom.NetworkType.kWiFi ||
        this.isPolicySource(managedProperties.source)) {
      return false;
    }
    const hexSsid = OncMojo.getActiveValue(managedProperties.wifi.hexSsid);
    return !!globalPolicy.AllowOnlyPolicyNetworksToConnect ||
        (!!globalPolicy.AllowOnlyPolicyNetworksToConnectIfAvailable &&
         !!managedNetworkAvailable) ||
        (typeof hexSsid == 'string' && !!globalPolicy.BlacklistedHexSSIDs &&
         globalPolicy.BlacklistedHexSSIDs.includes(hexSsid));
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  showConnect_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties) {
      return false;
    }

    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }

    // TODO(lgcheng@) support connect Arc VPN from UI once Android support API
    // to initiate a VPN session.
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }

    // If 'connectable' is false we show the configure button.
    return managedProperties.connectable &&
        managedProperties.type != mojom.NetworkType.kEthernet &&
        managedProperties.connectionState ==
        mojom.ConnectionStateType.kNotConnected;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showDisconnect_: function(managedProperties) {
    if (!managedProperties ||
        managedProperties.type == mojom.NetworkType.kEthernet) {
      return false;
    }
    return managedProperties.connectionState !=
        mojom.ConnectionStateType.kNotConnected;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showForget_: function(managedProperties) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    const type = managedProperties.type;
    if (type != mojom.NetworkType.kWiFi && type != mojom.NetworkType.kVPN) {
      return false;
    }
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }
    return !this.isPolicySource(managedProperties.source) &&
        this.isRemembered_(managedProperties);
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showActivate_: function(managedProperties) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    if (!this.isCellular_(managedProperties)) {
      return false;
    }
    const activation = managedProperties.cellular.activationState;
    return activation == mojom.ActivationStateType.kNotActivated ||
        activation == mojom.ActivationStateType.kPartiallyActivated;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  showConfigure_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    const type = managedProperties.type;
    if (type == mojom.NetworkType.kCellular ||
        type == mojom.NetworkType.kTether) {
      return false;
    }
    if (type == mojom.NetworkType.kWiFi &&
        managedProperties.wifi.security == mojom.SecurityType.kNone) {
      return false;
    }
    if (type == mojom.NetworkType.kWiFi &&
        (managedProperties.connectionState !=
         mojom.ConnectionStateType.kNotConnected)) {
      return false;
    }
    if (this.isArcVpn_(managedProperties) &&
        !this.isConnectedState_(managedProperties)) {
      return false;
    }
    return true;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.settingsPrivate.PrefObject} vpnConfigAllowed
   * @return {boolean}
   * @private
   */
  disableForget_: function(managedProperties, vpnConfigAllowed) {
    if (!managedProperties) {
      return true;
    }
    return managedProperties.type == mojom.NetworkType.kVPN &&
        vpnConfigAllowed && !vpnConfigAllowed.value;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.settingsPrivate.PrefObject} vpnConfigAllowed
   * @return {boolean}
   * @private
   */
  disableConfigure_: function(managedProperties, vpnConfigAllowed) {
    if (!managedProperties) {
      return true;
    }
    if (managedProperties.type == mojom.NetworkType.kVPN && vpnConfigAllowed &&
        !vpnConfigAllowed.value) {
      return true;
    }
    return this.isPolicySource(managedProperties.source) &&
        !this.hasRecommendedFields_(managedProperties);
  },


  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   */
  hasRecommendedFields_: function(managedProperties) {
    if (!managedProperties) {
      return false;
    }
    for (const key of Object.keys(managedProperties)) {
      const value = managedProperties[key];
      if (typeof value != 'object' || value === null) {
        continue;
      }
      if ('activeValue' in value) {
        if (this.isNetworkPolicyRecommended(value)) {
          return true;
        }
      } else if (this.hasRecommendedFields_(value)) {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showViewAccount_: function(managedProperties) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }

    // Show either the 'Activate' or the 'View Account' button (Cellular only).
    if (!this.isCellular_(managedProperties) ||
        this.showActivate_(managedProperties)) {
      return false;
    }

    const paymentPortal = managedProperties.cellular.paymentPortal;
    if (!paymentPortal || !paymentPortal.url) {
      return false;
    }

    // Only show for connected networks or LTE networks with a valid MDN.
    if (!this.isConnectedState_(managedProperties)) {
      const technology = managedProperties.cellular.networkTechnology;
      if (technology != CrOnc.NetworkTechnology.LTE &&
          technology != CrOnc.NetworkTechnology.LTE_ADVANCED) {
        return false;
      }
      if (!managedProperties.cellular.mdn) {
        return false;
      }
    }

    return true;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {?OncMojo.NetworkStateProperties} defaultNetwork
   * @param {boolean} networkPropertiesReceived
   * @param {boolean} outOfRange
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_: function(
      managedProperties, defaultNetwork, networkPropertiesReceived, outOfRange,
      globalPolicy, managedNetworkAvailable) {
    if (!this.showConnect_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    if (!networkPropertiesReceived || outOfRange) {
      return false;
    }
    if (managedProperties.type == mojom.NetworkType.kVPN && !defaultNetwork) {
      return false;
    }
    return true;
  },

  /** @private */
  updateAlwaysOnVpnPrefValue_: function() {
    this.alwaysOnVpn_.value = this.prefs.arc && this.prefs.arc.vpn &&
        this.prefs.arc.vpn.always_on && this.prefs.arc.vpn.always_on.lockdown &&
        this.prefs.arc.vpn.always_on.lockdown.value;
  },

  /**
   * @private
   * @return {!chrome.settingsPrivate.PrefObject}
   */
  getFakeVpnConfigPrefForEnforcement_: function() {
    const fakeAlwaysOnVpnEnforcementPref = {
      key: 'fakeAlwaysOnPref',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    // Only mark VPN networks as enforced. This fake pref also controls the
    // policy indicator on the connect/disconnect buttons, so it shouldn't be
    // shown on non-VPN networks.
    if (this.managedProperties_ &&
        this.managedProperties_.type == mojom.NetworkType.kVPN && this.prefs &&
        this.prefs.vpn_config_allowed && !this.prefs.vpn_config_allowed.value) {
      fakeAlwaysOnVpnEnforcementPref.enforcement =
          chrome.settingsPrivate.Enforcement.ENFORCED;
      fakeAlwaysOnVpnEnforcementPref.controlledBy =
          this.prefs.vpn_config_allowed.controlledBy;
    }
    return fakeAlwaysOnVpnEnforcementPref;
  },

  /** @private */
  updateAlwaysOnVpnPrefEnforcement_: function() {
    const prefForEnforcement = this.getFakeVpnConfigPrefForEnforcement_();
    this.alwaysOnVpn_.enforcement = prefForEnforcement.enforcement;
    this.alwaysOnVpn_.controlledBy = prefForEnforcement.controlledBy;
  },

  /**
   * @return {!TetherConnectionDialogElement}
   * @private
   */
  getTetherDialog_: function() {
    return /** @type {!TetherConnectionDialogElement} */ (this.$.tetherDialog);
  },

  /** @private */
  onConnectTap_: function() {
    // TODO(stevenjb): Add ManagedTetherProperties for hasConnectedToHost.
    // For Tether networks that have not connected to a host, show a dialog.
    if (this.networkProperties_.Type == CrOnc.Type.TETHER &&
        (!this.networkProperties_.Tether ||
         !this.networkProperties_.Tether.HasConnectedToHost)) {
      this.showTetherDialog_();
      return;
    }
    this.fireNetworkConnect_(/*bypassDialog=*/ false);
  },

  /** @private */
  onTetherConnect_: function() {
    this.getTetherDialog_().close();
    this.fireNetworkConnect_(/*bypassDialog=*/ true);
  },

  /**
   * @param {boolean} bypassDialog
   * @private
   */
  fireNetworkConnect_: function(bypassDialog) {
    assert(this.managedProperties_);
    const networkState =
        OncMojo.managedPropertiesToNetworkState(this.managedProperties_);
    this.fire(
        'network-connect',
        {networkState: networkState, bypassConnectionDialog: bypassDialog});
  },

  /** @private */
  onDisconnectTap_: function() {
    this.networkingPrivate.startDisconnect(this.guid);
  },

  /** @private */
  onForgetTap_: function() {
    this.networkingPrivate.forgetNetwork(this.guid);
    // A forgotten network no longer has a valid GUID, close the subpage.
    this.close();
  },

  /** @private */
  onActivateTap_: function() {
    this.networkingPrivate.startActivate(this.guid);
  },

  /** @private */
  onConfigureTap_: function() {
    if (this.managedProperties_ &&
        (this.isThirdPartyVpn_(this.managedProperties_) ||
         this.isArcVpn_(this.managedProperties_))) {
      this.browserProxy_.configureThirdPartyVpn(this.guid);
      return;
    }

    this.fire('show-config', {
      guid: this.guid,
      type: OncMojo.getNetworkTypeString(this.managedProperties_.type),
      name: OncMojo.getNetworkName(this.managedProperties_)
    });
  },

  /** @private */
  onViewAccountTap_: function() {
    // startActivate() will show the account page for activated networks.
    this.networkingPrivate.startActivate(this.guid);
  },

  /** @type {string} */
  CR_EXPAND_BUTTON_TAG: 'CR-EXPAND-BUTTON',

  /** @private */
  showTetherDialog_: function() {
    this.getTetherDialog_().open();
  },

  /**
   * @return {boolean}
   * @private
   */
  showHiddenNetworkWarning_: function() {
    return loadTimeData.getBoolean('showHiddenNetworkWarning') &&
        !!this.autoConnectPref_ && !!this.autoConnectPref_.value &&
        !!this.managedProperties_ &&
        !!this.managedProperties_.type == mojom.NetworkType.kWiFi &&
        !!OncMojo.getActiveValue(this.managedProperties_.wifi.hiddenSsid);
  },

  /**
   * Event triggered for elements associated with network properties.
   * @param {!CustomEvent<!{
   *     field: string,
   *     value: !CrOnc.NetworkPropertyType
   * }>} e
   * @private
   */
  onNetworkPropertyChange_: function(e) {
    if (!this.networkProperties_) {
      return;
    }
    const field = e.detail.field;
    const value = e.detail.value;
    const onc = this.getEmptyNetworkProperties_();
    const valueType = typeof value;
    if (valueType != 'string' && valueType != 'number' &&
        valueType != 'boolean' && !Array.isArray(value)) {
      console.error(
          'Unexpected property change event, Key: ' + field +
          ' Value: ' + JSON.stringify(value));
      return;
    }
    CrOnc.setProperty(onc, field, value);
    // Ensure any required configuration properties are also set.
    if (field.match(/^VPN/)) {
      const vpnType = CrOnc.getActiveValue(this.networkProperties_.VPN.Type);
      assert(vpnType);
      CrOnc.setProperty(onc, 'VPN.Type', vpnType);
    }
    this.setNetworkProperties_(onc);
  },

  /**
   * @param {!CustomEvent<!mojom.ApnProperties>} event
   * @private
   */
  onApnChange_: function(event) {
    if (!this.networkPropertiesReceived_) {
      return;
    }
    const apn = event.detail;
    const config = {cellular: {apn: apn}};
    this.setMojoNetworkProperties_(config);
  },


  /**
   * Event triggered when the IP Config or NameServers element changes.
   * @param {!CustomEvent<!{
   *     field: string,
   *     value: (string|!mojom.IPConfigProperties|!Array<string>)
   * }>} event The network-ip-config or network-nameservers change event.
   * @private
   */
  onIPConfigChange_: function(event) {
    if (!this.managedProperties_) {
      return;
    }
    const config = OncMojo.getUpdatedIPConfigProperties(
        this.managedProperties_, event.detail.field, event.detail.value);
    if (config) {
      this.setMojoNetworkProperties_(config);
    }
  },

  /**
   * Event triggered when the Proxy configuration element changes.
   * @param {!CustomEvent<!{field: string, value: !CrOnc.ProxySettings}>} event
   *     The network-proxy change event.
   * @private
   */
  onProxyChange_: function(event) {
    const field = event.detail.field;
    const value = event.detail.value;
    if (field != 'ProxySettings') {
      return;
    }
    const onc = this.getEmptyNetworkProperties_();
    CrOnc.setProperty(onc, 'ProxySettings', /** @type {!Object} */ (value));
    this.setNetworkProperties_(onc);
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the shared message should be shown.
   * @private
   */
  showShared_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    return !!managedProperties &&
        (managedProperties.source == mojom.OncSource.kDevice ||
         managedProperties.source == mojom.OncSource.kDevicePolicy) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the AutoConnect checkbox should be shown.
   * @private
   */
  showAutoConnect_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    return !!managedProperties &&
        managedProperties.type != mojom.NetworkType.kEthernet &&
        this.isRemembered_(managedProperties) &&
        !this.isArcVpn_(managedProperties) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean} Whether the toggle for the Always-on VPN feature is
   * displayed.
   * @private
   */
  showAlwaysOnVpn_: function(managedProperties) {
    return this.isArcVpn_(managedProperties) && this.prefs.arc &&
        this.prefs.arc.vpn && this.prefs.arc.vpn.always_on &&
        this.prefs.arc.vpn.always_on.vpn_package &&
        OncMojo.getActiveValue(managedProperties.vpn.host) ===
        this.prefs.arc.vpn.always_on.vpn_package.value;
  },

  /** @private */
  alwaysOnVpnChanged_: function() {
    if (this.prefs && this.prefs.arc && this.prefs.arc.vpn &&
        this.prefs.arc.vpn.always_on && this.prefs.arc.vpn.always_on.lockdown) {
      this.set(
          'prefs.arc.vpn.always_on.lockdown.value',
          !!this.alwaysOnVpn_ && this.alwaysOnVpn_.value);
    }
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the prefer network checkbox should be shown.
   * @private
   */
  showPreferNetwork_: function(
      networkProperties, managedProperties, globalPolicy,
      managedNetworkAvailable) {
    if (!networkProperties || !managedProperties) {
      return false;
    }

    const type = managedProperties.type;
    if (type == mojom.NetworkType.kEthernet ||
        type == mojom.NetworkType.kCellular ||
        this.isArcVpn_(managedProperties)) {
      return false;
    }

    return this.isRemembered_(managedProperties) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {Event} event
   * @private
   */
  onPreferNetworkRowClicked_: function(event) {
    // Stop propagation because the toggle and policy indicator handle clicks
    // themselves.
    event.stopPropagation();
    const preferNetworkToggle =
        this.shadowRoot.querySelector('#preferNetworkToggle');
    if (!preferNetworkToggle || preferNetworkToggle.disabled) {
      return;
    }

    this.preferNetwork_ = !this.preferNetwork_;
  },

  /**
   * @param {!Array<string>} fields
   * @return {boolean}
   * @private
   */
  hasVisibleFields_: function(fields) {
    for (let i = 0; i < fields.length; ++i) {
      const value = this.get(fields[i], this.networkProperties_);
      if (value !== undefined && value !== '') {
        return true;
      }
    }
    return false;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasInfoFields_: function() {
    return this.getInfoEditFieldTypes_().length > 0 ||
        this.hasVisibleFields_(this.getInfoFields_());
  },

  /**
   * @return {!Array<string>} The fields to display in the info section.
   * @private
   */
  getInfoFields_: function() {
    if (!this.networkProperties_) {
      return [];
    }

    /** @type {!Array<string>} */ const fields = [];
    const type = this.networkProperties_.Type;
    if (type == CrOnc.Type.CELLULAR && !!this.networkProperties_.Cellular) {
      fields.push(
          'Cellular.ActivationState', 'RestrictedConnectivity',
          'Cellular.ServingOperator.Name');
    } else if (type == CrOnc.Type.TETHER && !!this.networkProperties_.Tether) {
      fields.push(
          'Tether.BatteryPercentage', 'Tether.SignalStrength',
          'Tether.Carrier');
    } else if (type == CrOnc.Type.VPN && !!this.networkProperties_.VPN) {
      const vpnType = CrOnc.getActiveValue(this.networkProperties_.VPN.Type);
      switch (vpnType) {
        case CrOnc.VPNType.THIRD_PARTY_VPN:
          fields.push('VPN.ThirdPartyVPN.ProviderName');
          break;
        case CrOnc.VPNType.ARCVPN:
          fields.push('VPN.Type');
          break;
        case CrOnc.VPNType.OPEN_VPN:
          fields.push(
              'VPN.Type', 'VPN.Host', 'VPN.OpenVPN.Username',
              'VPN.OpenVPN.ExtraHosts');
          break;
        case CrOnc.VPNType.L2TP_IPSEC:
          fields.push('VPN.Type', 'VPN.Host', 'VPN.L2TP.Username');
          break;
      }
    } else if (type == CrOnc.Type.WI_FI) {
      fields.push('RestrictedConnectivity');
    }
    return fields;
  },

  /**
   * @return {!Object} A dictionary of editable fields in the info section.
   * @private
   */
  getInfoEditFieldTypes_: function() {
    if (!this.networkProperties_) {
      return [];
    }

    /** @dict */ const editFields = {};
    const type = this.networkProperties_.Type;
    if (type == CrOnc.Type.VPN && !!this.networkProperties_.VPN) {
      const vpnType = CrOnc.getActiveValue(this.networkProperties_.VPN.Type);
      if (vpnType != CrOnc.VPNType.THIRD_PARTY_VPN) {
        editFields['VPN.Host'] = 'String';
      }
      if (vpnType == CrOnc.VPNType.OPEN_VPN) {
        editFields['VPN.OpenVPN.Username'] = 'String';
        editFields['VPN.OpenVPN.ExtraHosts'] = 'StringArray';
      }
    }
    return editFields;
  },

  /**
   * @return {!Array<string>} The fields to display in the Advanced section.
   * @private
   */
  getAdvancedFields_: function() {
    if (!this.networkProperties_) {
      return [];
    }

    /** @type {!Array<string>} */ const fields = [];
    const type = this.networkProperties_.Type;
    if (type != CrOnc.Type.TETHER) {
      fields.push('MacAddress');
    }
    if (type == CrOnc.Type.CELLULAR && !!this.networkProperties_.Cellular) {
      fields.push(
          'Cellular.Family', 'Cellular.NetworkTechnology',
          'Cellular.ServingOperator.Code');
    } else if (type == CrOnc.Type.WI_FI) {
      fields.push(
          'WiFi.SSID', 'WiFi.BSSID', 'WiFi.SignalStrength', 'WiFi.Security',
          'WiFi.EAP.Outer', 'WiFi.EAP.Inner', 'WiFi.EAP.SubjectMatch',
          'WiFi.EAP.Identity', 'WiFi.EAP.AnonymousIdentity', 'WiFi.Frequency');
    }
    return fields;
  },

  /**
   * @return {!Array<string>} The fields to display in the device section.
   * @private
   */
  getDeviceFields_: function() {
    if (!this.networkProperties_ ||
        this.networkProperties_.Type !== CrOnc.Type.CELLULAR) {
      return [];
    }

    return [
      'Cellular.HomeProvider.Name', 'Cellular.HomeProvider.Country',
      'Cellular.HomeProvider.Code', 'Cellular.Manufacturer', 'Cellular.ModelID',
      'Cellular.FirmwareRevision', 'Cellular.HardwareRevision', 'Cellular.ESN',
      'Cellular.ICCID', 'Cellular.IMEI', 'Cellular.IMSI', 'Cellular.MDN',
      'Cellular.MEID', 'Cellular.MIN'
    ];
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showAdvanced_: function(networkProperties, managedProperties) {
    if (!managedProperties ||
        managedProperties.type == mojom.NetworkType.kTether) {
      // These settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    return this.hasAdvancedFields_() || this.hasDeviceFields_() ||
        (managedProperties.type != mojom.NetworkType.kVPN &&
         this.isRememberedOrConnected_(managedProperties));
  },

  /**
   * @return {boolean}
   * @private
   */
  hasAdvancedFields_: function() {
    return this.hasVisibleFields_(this.getAdvancedFields_());
  },

  /**
   * @return {boolean}
   * @private
   */
  hasDeviceFields_: function() {
    return this.hasVisibleFields_(this.getDeviceFields_());
  },

  /**
   * @return {boolean}
   * @private
   */
  hasAdvancedOrDeviceFields_: function() {
    return this.hasAdvancedFields_() || this.hasDeviceFields_();
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  hasNetworkSection_: function(
      networkProperties, managedProperties, globalPolicy,
      managedNetworkAvailable) {
    if (!networkProperties || !managedProperties ||
        managedProperties.type == mojom.NetworkType.kTether) {
      // These settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    if (managedProperties.type == mojom.NetworkType.kCellular) {
      return true;
    }
    return this.isRememberedOrConnected_(managedProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  hasProxySection_: function(
      networkProperties, managedProperties, globalPolicy,
      managedNetworkAvailable) {
    if (!networkProperties || !managedProperties ||
        managedProperties.type == mojom.NetworkType.kTether) {
      // Proxy settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    return this.isRememberedOrConnected_(managedProperties);
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showCellularChooseNetwork_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kCellular &&
        managedProperties.cellular.supportNetworkScan;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showScanningSpinner_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kCellular &&
        managedProperties.cellular.scanning;
  },

  /**
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showCellularSim_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kCellular &&
        managedProperties.cellular.family != 'CDMA';
  },

  /**
   * @param {!OncMojo.ManagedProperties|undefined} managedProperties
   * @return {boolean}
   * @private
   */
  isArcVpn_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kVPN &&
        managedProperties.vpn.type == mojom.VPNType.kArcVPN;
  },

  /**
   * @param {!OncMojo.ManagedProperties|undefined} managedProperties
   * @return {boolean}
   * @private
   */
  isThirdPartyVpn_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kVPN &&
        managedProperties.vpn.type == mojom.VPNType.kThirdPartyVPN;
  },

  /**
   * @param {string} ipAddress
   * @param {!OncMojo.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showIpAddress_: function(ipAddress, managedProperties) {
    // Arc Vpn does not currently pass IP configuration to ChromeOS. IP address
    // property holds an internal IP address Android uses to talk to ChromeOS.
    // TODO(lgcheng@) Show correct IP address when we implement IP configuration
    // correctly.
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }

    // Cellular IP addresses are shown under the network details section.
    if (this.isCellular_(managedProperties)) {
      return false;
    }

    return !!ipAddress && this.isConnectedState_(managedProperties);
  },

  /**
   * @param {!Object} curValue
   * @param {!Object} newValue
   * @return {boolean} True if all properties set in |newValue| are equal to
   *     the corresponding properties in |curValue|. Note: Not all properties
   *     of |curValue| need to be specified in |newValue| for this to return
   *     true.
   * @private
   */
  allPropertiesMatch_: function(curValue, newValue) {
    for (const key in newValue) {
      if (newValue[key] != curValue[key]) {
        return false;
      }
    }
    return true;
  },
});
})();
