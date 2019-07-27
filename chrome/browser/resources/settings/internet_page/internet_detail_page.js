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
    CrPolicyNetworkBehavior,
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
      observer: 'networkPropertiesChanged_',
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
      observer: 'globalPolicyChanged_',
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
    autoConnect_: {
      type: Object,
      observer: 'autoConnectChanged_',
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
    'updateAlwaysOnVpnPrefEnforcement_(prefs.vpn_config_allowed.*)',
    'autoConnectChanged_(autoConnect_.*)', 'alwaysOnVpnChanged_(alwaysOnVpn_.*)'
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
   * @private
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
      this.networkPropertiesReceived_ = false;

      settings.navigateToPreviousRoute();
    });
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged: function(networks) {
    if (!this.guid || !this.networkProperties_) {
      return;
    }
    // If the network was or is active, request an update.
    if (this.networkProperties_.ConnectionState !=
            CrOnc.ConnectionState.NOT_CONNECTED ||
        networks.find(network => network.guid == this.guid)) {
      this.getNetworkDetails_();
    }
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.checkNetworkExists_();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    if (this.guid) {
      this.getNetworkDetails_();
    }
  },

  /**
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @private
   */
  globalPolicyChanged_: function(globalPolicy) {
    this.updateAutoConnectPref_(
        !!(this.autoConnect_ && this.autoConnect_.value), globalPolicy);
  },

  /** @private */
  networkPropertiesChanged_: function() {
    if (!this.networkProperties_) {
      return;
    }

    // Update autoConnect if it has changed. Default value is false.
    this.updateAutoConnectPref_(
        CrOnc.getAutoConnect(this.networkProperties_), this.globalPolicy);

    // Update preferNetwork if it has changed. Default value is false.
    const priority = /** @type {number} */ (
        CrOnc.getActiveValue(this.networkProperties_.Priority) || 0);
    const preferNetwork = priority > 0;
    if (preferNetwork != this.preferNetwork_) {
      this.preferNetwork_ = preferNetwork;
    }

    // Set the IPAddress property to the IPV4 Address.
    const ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties_, CrOnc.IPType.IPV4);
    this.ipAddress_ = (ipv4 && ipv4.IPAddress) || '';

    // Update the detail page title.
    this.parentNode.pageTitle = CrOnc.getNetworkName(this.networkProperties_);

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
        this.networkProperties_.Tether) {
      // Set |this.shouldShowConfigureWhenNetworkLoaded_| back to false to
      // ensure that the Tether dialog is only shown once.
      this.shouldShowConfigureWhenNetworkLoaded_ = false;
      this.showTetherDialog_();
    }
  },

  /** @private */
  autoConnectChanged_: function() {
    if (!this.networkProperties_ || !this.guid) {
      return;
    }
    const onc = this.getEmptyNetworkProperties_();
    CrOnc.setTypeProperty(onc, 'AutoConnect', !!this.autoConnect_.value);
    this.setNetworkProperties_(onc);
  },

  /**
   * Updates auto-connect pref value.
   * @param {boolean} value
   * @param {!chrome.networkingPrivate.GlobalPolicy|undefined} globalPolicy
   * @private
   */
  updateAutoConnectPref_: function(value, globalPolicy) {
    let enforcement;
    let controlledBy;

    if (this.isAutoConnectEnforcedByPolicy(
            this.networkProperties_, globalPolicy)) {
      enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
    }

    if (this.autoConnect_ && this.autoConnect_.value == value &&
        enforcement == this.autoConnect_.enforcement &&
        controlledBy == this.autoConnect_.controlledBy) {
      return;
    }

    const newPrefValue = {
      key: 'fakeAutoConnectPref',
      value: value,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    if (enforcement) {
      newPrefValue.enforcement = enforcement;
      newPrefValue.controlledBy = controlledBy;
    }

    this.autoConnect_ = newPrefValue;
  },

  /** @private */
  preferNetworkChanged_: function() {
    if (!this.networkProperties_ || !this.guid) {
      return;
    }
    const onc = this.getEmptyNetworkProperties_();
    onc.Priority = this.preferNetwork_ ? 1 : 0;
    this.setNetworkProperties_(onc);
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
      if (this.networkProperties_) {
        // Set the connection state since we won't receive an update for a non
        // existent network.
        this.networkProperties_.ConnectionState =
            CrOnc.ConnectionState.NOT_CONNECTED;
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
      this.networkConfigProxy_.getNetworkState(this.guid).then(response => {
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

    // Detail page should not be shown when Arc VPN is not connected.
    if (this.isArcVpn_(properties) && !this.isConnectedState_(properties)) {
      this.guid = '';
      this.close();
    }

    this.networkProperties_ = properties;
    this.networkPropertiesReceived_ = true;
    this.outOfRange_ = false;
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
    this.networkPropertiesReceived_ = true;
    this.outOfRange_ = false;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {!OncMojo.NetworkStateProperties|undefined}
   */
  getNetworkState_: function(properties) {
    return properties ? OncMojo.oncPropertiesToNetworkState(properties) :
                        undefined;
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} onc The ONC
   *     network properties.
   * @private
   */
  setNetworkProperties_: function(onc) {
    if (!this.networkPropertiesReceived_) {
      return;
    }

    assert(this.guid);
    this.networkingPrivate.setProperties(this.guid, onc, () => {
      if (chrome.runtime.lastError) {
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
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {boolean} outOfRange
   * @return {string} The text to display for the network connection state.
   * @private
   */
  getStateText_: function(networkProperties, outOfRange) {
    if (!networkProperties || !networkProperties.ConnectionState) {
      return '';
    }

    if (outOfRange) {
      return networkProperties.Type == CrOnc.Type.TETHER ?
          this.i18n('tetherPhoneOutOfRange') :
          this.i18n('networkOutOfRange');
    }

    return this.i18n('Onc' + networkProperties.ConnectionState);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {string} The text to display for auto-connect toggle label.
   * @private
   */
  getAutoConnectToggleLabel_: function(networkProperties) {
    return this.isCellular_(networkProperties) ?
        this.i18n('networkAutoConnectCellular') :
        this.i18n('networkAutoConnect');
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {string} The text to display with roaming details.
   * @private
   */
  getRoamingDetails_: function(networkProperties) {
    if (!networkProperties.Cellular.AllowRoaming) {
      return this.i18n('networkAllowDataRoamingDisabled');
    }

    return networkProperties.Cellular.RoamingState ===
            CrOnc.RoamingState.ROAMING ?
        this.i18n('networkAllowDataRoamingEnabledRoaming') :
        this.i18n('networkAllowDataRoamingEnabledHome');
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean} True if the network is connected.
   * @private
   */
  isConnectedState_: function(networkProperties) {
    return !!networkProperties &&
        networkProperties.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  isRemembered_: function(networkProperties) {
    const source = networkProperties ? networkProperties.Source : null;
    return !!source && source != CrOnc.Source.NONE;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  isRememberedOrConnected_: function(networkProperties) {
    return this.isRemembered_(networkProperties) ||
        this.isConnectedState_(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  isCellular_: function(networkProperties) {
    return !!networkProperties &&
        networkProperties.Type == CrOnc.Type.CELLULAR &&
        !!networkProperties.Cellular;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_: function(
      networkProperties, globalPolicy, managedNetworkAvailable) {
    if (!networkProperties || networkProperties.Type != CrOnc.Type.WI_FI ||
        this.isPolicySource(networkProperties.Source) || !globalPolicy) {
      return false;
    }
    return !!globalPolicy.AllowOnlyPolicyNetworksToConnect ||
        (!!globalPolicy.AllowOnlyPolicyNetworksToConnectIfAvailable &&
         !!managedNetworkAvailable) ||
        (!!networkProperties.WiFi && !!networkProperties.WiFi.HexSSID &&
         !!globalPolicy.BlacklistedHexSSIDs &&
         globalPolicy.BlacklistedHexSSIDs.includes(
             CrOnc.getStateOrActiveString(networkProperties.WiFi.HexSSID)));
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  showConnect_: function(
      networkProperties, globalPolicy, managedNetworkAvailable) {
    if (!networkProperties) {
      return false;
    }

    if (this.isBlockedByPolicy_(
            networkProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    // TODO(lgcheng@) support connect Arc VPN from UI once Android support API
    // to initiate a VPN session.
    if (this.isArcVpn_(networkProperties)) {
      return false;
    }

    return networkProperties.Type != CrOnc.Type.ETHERNET &&
        networkProperties.ConnectionState ==
        CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showDisconnect_: function(networkProperties) {
    return !!networkProperties &&
        networkProperties.Type != CrOnc.Type.ETHERNET &&
        CrOnc.isConnectingOrConnected(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showForget_: function(networkProperties) {
    if (this.isSecondaryUser_ || !networkProperties) {
      return false;
    }
    const type = networkProperties.Type;
    if (type != CrOnc.Type.WI_FI && type != CrOnc.Type.VPN) {
      return false;
    }
    if (this.isArcVpn_(networkProperties)) {
      return false;
    }
    return !this.isPolicySource(networkProperties.Source) &&
        this.isRemembered_(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showActivate_: function(networkProperties) {
    if (this.isSecondaryUser_) {
      return false;
    }
    if (!this.isCellular_(networkProperties)) {
      return false;
    }
    const activation = networkProperties.Cellular.ActivationState;
    return activation == CrOnc.ActivationState.NOT_ACTIVATED ||
        activation == CrOnc.ActivationState.PARTIALLY_ACTIVATED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  showConfigure_: function(
      networkProperties, globalPolicy, managedNetworkAvailable) {
    if (this.isSecondaryUser_ || !networkProperties) {
      return false;
    }
    if (this.isBlockedByPolicy_(
            networkProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    const type = networkProperties.Type;
    if (type == CrOnc.Type.CELLULAR || type == CrOnc.Type.TETHER) {
      return false;
    }
    if (type == CrOnc.Type.WI_FI) {
      const security = networkProperties.WiFi &&
          CrOnc.getActiveValue(networkProperties.WiFi.Security);
      if (!security || security == CrOnc.Security.NONE) {
        return false;
      }
    }
    if ((type == CrOnc.Type.WI_FI || type == CrOnc.Type.WI_MAX) &&
        CrOnc.isConnectingOrConnected(networkProperties)) {
      return false;
    }
    if (this.isArcVpn_(networkProperties) &&
        !this.isConnectedState_(networkProperties)) {
      return false;
    }
    return true;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!chrome.settingsPrivate.PrefObject} vpn_config_allowed
   * @return {boolean}
   * @private
   */
  disableForget_: function(networkProperties, vpn_config_allowed) {
    return this.isVpn_(networkProperties) && vpn_config_allowed &&
        !vpn_config_allowed.value;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!chrome.settingsPrivate.PrefObject} vpn_config_allowed
   * @return {boolean}
   * @private
   */
  disableConfigure_: function(networkProperties, vpn_config_allowed) {
    if (!networkProperties ||
        (this.isVpn_(networkProperties) && vpn_config_allowed &&
         !vpn_config_allowed.value)) {
      return true;
    }
    return this.isPolicySource(networkProperties.Source) &&
        !this.hasRecommendedFields_(networkProperties);
  },


  /**
   * @param {!Object} networkProperties
   * @return {boolean}
   */
  hasRecommendedFields_: function(networkProperties) {
    for (const property in networkProperties) {
      const propertyValue = networkProperties[property];
      if (this.isNetworkPolicyRecommended(propertyValue) ||
          (typeof propertyValue == 'object' &&
           this.hasRecommendedFields_(propertyValue))) {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showViewAccount_: function(networkProperties) {
    if (this.isSecondaryUser_) {
      return false;
    }

    // Show either the 'Activate' or the 'View Account' button (Cellular only).
    if (!this.isCellular_(networkProperties) ||
        this.showActivate_(networkProperties)) {
      return false;
    }

    const paymentPortal = networkProperties.Cellular.PaymentPortal;
    if (!paymentPortal || !paymentPortal.Url) {
      return false;
    }

    // Only show for connected networks or LTE networks with a valid MDN.
    if (!this.isConnectedState_(networkProperties)) {
      const technology = networkProperties.Cellular.NetworkTechnology;
      if (technology != CrOnc.NetworkTechnology.LTE &&
          technology != CrOnc.NetworkTechnology.LTE_ADVANCED) {
        return false;
      }
      if (!networkProperties.Cellular.MDN) {
        return false;
      }
    }

    return true;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {?OncMojo.NetworkStateProperties} defaultNetwork
   * @param {boolean} networkPropertiesReceived
   * @param {boolean} outOfRange
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_: function(
      networkProperties, defaultNetwork, networkPropertiesReceived, outOfRange,
      globalPolicy, managedNetworkAvailable) {
    if (!this.showConnect_(
            networkProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    if (!networkPropertiesReceived || outOfRange) {
      return false;
    }
    if (this.isSecondaryUser_ &&
        this.networkProperties_.Connectable === false) {
      return false;
    }
    if ((networkProperties.Type == CrOnc.Type.CELLULAR) &&
        (CrOnc.isSimLocked(networkProperties) ||
         this.get('Cellular.Scanning', networkProperties))) {
      return false;
    }
    if (networkProperties.Type == CrOnc.Type.VPN && !defaultNetwork) {
      return false;
    }
    return true;
  },

  /**
   * @param {!CrOnc.NetworkProperties=} networkProperties
   * @return {boolean} Whether or not we are looking at VPN configuration.
   * @private
   */
  isVpn_: function(networkProperties) {
    return !!networkProperties && networkProperties.Type == CrOnc.Type.VPN;
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
    if (this.isVpn_(this.networkProperties_) && this.prefs &&
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
    assert(this.networkProperties_);
    const networkState =
        OncMojo.oncPropertiesToNetworkState(this.networkProperties_);
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
    if (this.networkProperties_ &&
        (this.isThirdPartyVpn_(this.networkProperties_) ||
         this.isArcVpn_(this.networkProperties_))) {
      this.browserProxy_.configureThirdPartyVpn(this.guid);
      return;
    }

    this.fire('show-config', {
      guid: this.networkProperties_.GUID,
      type: this.networkProperties_.Type,
      name: CrOnc.getNetworkName(this.networkProperties_)
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
        !!this.autoConnect_ && !!this.autoConnect_.value &&
        !!this.networkProperties_ && !!this.networkProperties_.WiFi &&
        !!CrOnc.getActiveValue(this.networkProperties_.WiFi.HiddenSSID);
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
    if (field == 'APN') {
      CrOnc.setTypeProperty(onc, 'APN', value);
    } else if (field == 'SIMLockStatus') {
      CrOnc.setTypeProperty(onc, 'SIMLockStatus', value);
    } else {
      const valueType = typeof value;
      if (valueType == 'string' || valueType == 'number' ||
          valueType == 'boolean' || Array.isArray(value)) {
        CrOnc.setProperty(onc, field, value);
        // Ensure any required configuration properties are also set.
        if (field.match(/^VPN/)) {
          const vpnType =
              CrOnc.getActiveValue(this.networkProperties_.VPN.Type);
          assert(vpnType);
          CrOnc.setProperty(onc, 'VPN.Type', vpnType);
        }
      } else {
        console.error(
            'Unexpected property change event, Key: ' + field +
            ' Value: ' + JSON.stringify(value));
        return;
      }
    }
    this.setNetworkProperties_(onc);
  },

  /**
   * Event triggered when the IP Config or NameServers element changes.
   * @param {!CustomEvent<!{
   *     field: string,
   *     value: (string|!CrOnc.IPConfigProperties| !Array<string>)
   * }>} event The network-ip-config or network-nameservers change event.
   * @private
   */
  onIPConfigChange_: function(event) {
    if (!this.networkProperties_) {
      return;
    }
    const field = event.detail.field;
    const value = event.detail.value;
    // Get an empty ONC dictionary and set just the IP Config properties that
    // need to change.
    const onc = this.getEmptyNetworkProperties_();
    const ipConfigType =
        /** @type {chrome.networkingPrivate.IPConfigType|undefined} */ (
            CrOnc.getActiveValue(this.networkProperties_.IPAddressConfigType));
    if (field == 'IPAddressConfigType') {
      const newIpConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */ (value);
      if (newIpConfigType == ipConfigType) {
        return;
      }
      onc.IPAddressConfigType = newIpConfigType;
    } else if (field == 'NameServersConfigType') {
      const nsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType|undefined} */ (
              CrOnc.getActiveValue(
                  this.networkProperties_.NameServersConfigType));
      const newNsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */ (value);
      if (newNsConfigType == nsConfigType) {
        return;
      }
      onc.NameServersConfigType = newNsConfigType;
    } else if (field == 'StaticIPConfig') {
      if (ipConfigType == CrOnc.IPConfigType.STATIC) {
        const staticIpConfig = this.networkProperties_.StaticIPConfig;
        const ipConfigValue = /** @type {!Object} */ (value);
        if (staticIpConfig &&
            this.allPropertiesMatch_(staticIpConfig, ipConfigValue)) {
          return;
        }
      }
      onc.IPAddressConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */ ({});
      }
      // Only copy Static IP properties.
      const keysToCopy = ['Type', 'IPAddress', 'RoutingPrefix', 'Gateway'];
      for (let i = 0; i < keysToCopy.length; ++i) {
        const key = keysToCopy[i];
        if (key in value) {
          onc.StaticIPConfig[key] = value[key];
        }
      }
    } else if (field == 'NameServers') {
      // If a StaticIPConfig property is specified and its NameServers value
      // matches the new value, no need to set anything.
      const nameServers = /** @type {!Array<string>} */ (value);
      if (onc.NameServersConfigType == CrOnc.IPConfigType.STATIC &&
          onc.StaticIPConfig && onc.StaticIPConfig.NameServers == nameServers) {
        return;
      }
      onc.NameServersConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */ ({});
      }
      onc.StaticIPConfig.NameServers = nameServers;
    } else {
      console.error('Unexpected change field: ' + field);
      return;
    }
    // setValidStaticIPConfig will fill in any other properties from
    // networkProperties. This is necessary since we update IP Address and
    // NameServers independently.
    CrOnc.setValidStaticIPConfig(onc, this.networkProperties_);
    this.setNetworkProperties_(onc);
  },

  /**
   * Event triggered when the Proxy configuration element changes.
   * @param {!CustomEvent<!{field: string, value: !CrOnc.ProxySettings}>} event
   *     The network-proxy change event.
   * @private
   */
  onProxyChange_: function(event) {
    if (!this.networkProperties_) {
      return;
    }
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
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the shared message should be shown.
   * @private
   */
  showShared_: function(
      networkProperties, globalPolicy, managedNetworkAvailable) {
    return !!networkProperties &&
        (networkProperties.Source == 'Device' ||
         networkProperties.Source == 'DevicePolicy') &&
        !this.isBlockedByPolicy_(
            networkProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the AutoConnect checkbox should be shown.
   * @private
   */
  showAutoConnect_: function(
      networkProperties, globalPolicy, managedNetworkAvailable) {
    return !!networkProperties &&
        networkProperties.Type != CrOnc.Type.ETHERNET &&
        this.isRemembered_(networkProperties) &&
        !this.isArcVpn_(networkProperties) &&
        !this.isBlockedByPolicy_(
            networkProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {!CrOnc.NetworkProperties=} networkProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy=} globalPolicy
   * @return {boolean}
   * @private
   */
  isAutoConnectEnforcedByPolicy: function(networkProperties, globalPolicy) {
    if (!networkProperties || networkProperties.Type != CrOnc.Type.WI_FI) {
      return false;
    }
    if (this.isPolicySource(networkProperties.Source)) {
      return !this.isEditable(CrOnc.getManagedAutoConnect(networkProperties));
    }
    return !!globalPolicy &&
        !!globalPolicy.AllowOnlyPolicyNetworksToAutoconnect;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean} Whether the toggle for the Always-on VPN feature is
   * displayed.
   * @private
   */
  showAlwaysOnVpn_: function(networkProperties) {
    return this.isArcVpn_(networkProperties) && this.prefs.arc &&
        this.prefs.arc.vpn && this.prefs.arc.vpn.always_on &&
        this.prefs.arc.vpn.always_on.vpn_package &&
        networkProperties.VPN.Host.Active ===
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
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the prefer network checkbox should be shown.
   * @private
   */
  showPreferNetwork_: function(
      networkProperties, globalPolicy, managedNetworkAvailable) {
    if (!networkProperties) {
      return false;
    }

    const type = networkProperties.Type;
    if (type == CrOnc.Type.ETHERNET || type == CrOnc.Type.CELLULAR ||
        this.isArcVpn_(networkProperties)) {
      return false;
    }

    return this.isRemembered_(networkProperties) &&
        !this.isBlockedByPolicy_(
            networkProperties, globalPolicy, managedNetworkAvailable);
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
    } else if (type == CrOnc.Type.WI_MAX) {
      fields.push('RestrictedConnectivity', 'WiMAX.EAP.Identity');
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
    } else if (type == CrOnc.Type.WI_MAX) {
      fields.push('WiFi.SignalStrength');
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
      'Cellular.MEID', 'Cellular.MIN', 'Cellular.PRLVersion'
    ];
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showAdvanced_: function(networkProperties) {
    if (!networkProperties || networkProperties.Type == CrOnc.Type.TETHER) {
      // These settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    return this.hasAdvancedFields_() || this.hasDeviceFields_() ||
        (networkProperties.Type != CrOnc.Type.VPN &&
         this.isRememberedOrConnected_(networkProperties));
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
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  hasNetworkSection_: function(
      networkProperties, globalPolicy, managedNetworkAvailable) {
    if (!networkProperties || networkProperties.Type == CrOnc.Type.TETHER) {
      // These settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            networkProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    if (networkProperties.Type == CrOnc.Type.CELLULAR) {
      return true;
    }
    return this.isRememberedOrConnected_(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  hasProxySection_: function(
      networkProperties, globalPolicy, managedNetworkAvailable) {
    if (!networkProperties || networkProperties.Type == CrOnc.Type.TETHER) {
      // Proxy settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            networkProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    return this.isRememberedOrConnected_(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showCellularChooseNetwork_: function(networkProperties) {
    return !!networkProperties &&
        networkProperties.Type == CrOnc.Type.CELLULAR &&
        !!this.get('Cellular.SupportNetworkScan', this.networkProperties_);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showScanningSpinner_: function(networkProperties) {
    return !!this.get('Cellular.Scanning', networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showCellularSim_: function(networkProperties) {
    return !!networkProperties &&
        networkProperties.Type == CrOnc.Type.CELLULAR &&
        !!networkProperties.Cellular &&
        networkProperties.Cellular.Family != 'CDMA';
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  isArcVpn_: function(networkProperties) {
    return !!networkProperties && !!networkProperties.VPN &&
        CrOnc.getActiveValue(networkProperties.VPN.Type) ==
        CrOnc.VPNType.ARCVPN;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  isThirdPartyVpn_: function(networkProperties) {
    return !!networkProperties && !!networkProperties.VPN &&
        CrOnc.getActiveValue(networkProperties.VPN.Type) ==
        CrOnc.VPNType.THIRD_PARTY_VPN;
  },

  /**
   * @param {string} ipAddress
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showIpAddress_: function(ipAddress, networkProperties) {
    // Arc Vpn does not currently pass IP configuration to ChromeOS. IP address
    // property holds an internal IP address Android uses to talk to ChromeOS.
    // TODO(lgcheng@) Show correct IP address when we implement IP configuration
    // correctly.
    if (this.isArcVpn_(networkProperties)) {
      return false;
    }

    // Cellular IP addresses are shown under the network details section.
    if (this.isCellular_(networkProperties)) {
      return false;
    }

    return !!ipAddress && this.isConnectedState_(networkProperties);
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
