// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the IP Config properties for
 * a network state.
 */
(function() {
'use strict';

Polymer({
  is: 'network-ip-config',

  behaviors: [I18nBehavior, CrPolicyNetworkBehaviorMojo],

  properties: {
    /** @private {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: {
      type: Object,
      observer: 'managedPropertiesChanged_',
    },

    /**
     * State of 'Configure IP Addresses Automatically'.
     * @private
     */
    automatic_: {
      type: Boolean,
      value: true,
    },

    /**
     * The currently visible IP Config property dictionary.
     * @private {{
     *   ipv4: (OncMojo.IPConfigUIProperties|undefined),
     *   ipv6: (OncMojo.IPConfigUIProperties|undefined)
     * }|undefined}
     */
    ipConfig_: Object,

    /**
     * Array of properties to pass to the property list.
     * @private {!Array<string>}
     */
    ipConfigFields_: {
      type: Array,
      value: function() {
        return [
          'ipv4.ipAddress',
          'ipv4.routingPrefix',
          'ipv4.gateway',
          'ipv6.ipAddress',
        ];
      },
      readOnly: true
    },
  },

  /**
   * Saved static IP configuration properties when switching to 'automatic'.
   * @private {!OncMojo.IPConfigUIProperties|undefined}
   */
  savedStaticIp_: undefined,

  /** @private */
  managedPropertiesChanged_: function(newValue, oldValue) {
    if (!this.managedProperties) {
      return;
    }

    const properties = this.managedProperties;
    if (newValue.guid != (oldValue && oldValue.guid)) {
      this.savedStaticIp_ = undefined;
    }

    // Update the 'automatic' property.
    if (properties.ipAddressConfigType) {
      const ipConfigType =
          OncMojo.getActiveValue(properties.ipAddressConfigType);
      this.automatic_ = ipConfigType != 'Static';
    }

    if (properties.ipConfigs || properties.staticIpConfig) {
      // Update the 'ipConfig' property.
      const ipv4 = this.getIPConfigUIProperties_(
          OncMojo.getIPConfigForType(properties, 'IPv4'));
      let ipv6 = this.getIPConfigUIProperties_(
          OncMojo.getIPConfigForType(properties, 'IPv6'));
      if (OncMojo.connectionStateIsConnected(properties.connectionState) &&
          ipv4 && ipv4.ipAddress) {
        ipv6 = ipv6 || {};
        ipv6.ipAddress = ipv6.ipAddress || this.i18n('loading');
      }
      this.ipConfig_ = {ipv4: ipv4, ipv6: ipv6};
    } else {
      this.ipConfig_ = undefined;
    }
  },

  /**
   * Checks whether IP address config type can be changed.
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  canChangeIPConfigType_: function(managedProperties) {
    const ipConfigType = managedProperties.ipAddressConfigType;
    return !ipConfigType || !this.isNetworkPolicyEnforced(ipConfigType);
  },

  /** @private */
  onAutomaticChange_: function() {
    if (!this.automatic_) {
      const defaultIpv4 = {
        gateway: '192.168.1.1',
        ipAddress: '192.168.1.1',
        routingPrefix: '255.255.255.0',
        type: 'IPv4',
      };
      // Ensure that there is a valid IPConfig object. Copy any set properties
      // over the default properties to ensure all properties are set.
      if (this.ipConfig_) {
        this.ipConfig_.ipv4 = Object.assign(defaultIpv4, this.ipConfig_.ipv4);
      } else {
        this.ipConfig_ = {ipv4: defaultIpv4};
      }
      this.sendStaticIpConfig_();
      return;
    }

    // Save the static IP configuration when switching to automatic.
    if (this.ipConfig_) {
      this.savedStaticIp_ = this.ipConfig_.ipv4;
    }
    this.fire('ip-change', {
      field: 'ipAddressConfigType',
      value: 'DHCP',
    });
  },

  /**
   * @param {!chromeos.networkConfig.mojom.IPConfigProperties|undefined}
   *     ipconfig
   * @return {!OncMojo.IPConfigUIProperties|undefined} A new
   *     IPConfigUIProperties object with routingPrefix expressed as a string
   *     mask instead of a prefix length. Returns undefined if |ipconfig| is not
   *     defined.
   * @private
   */
  getIPConfigUIProperties_: function(ipconfig) {
    if (!ipconfig) {
      return undefined;
    }
    const result = {};
    for (const key in ipconfig) {
      const value = ipconfig[key];
      if (key == 'routingPrefix') {
        result.routingPrefix = CrOnc.getRoutingPrefixAsNetmask(value);
      } else {
        result[key] = value;
      }
    }
    return result;
  },

  /**
   * @param {!OncMojo.IPConfigUIProperties} ipconfig
   * @return {!chromeos.networkConfig.mojom.IPConfigProperties} A new
   *     IPConfigProperties object with RoutingPrefix expressed as a a prefix
   *     length.
   * @private
   */
  getIPConfigProperties_: function(ipconfig) {
    const result = {};
    for (const key in ipconfig) {
      const value = ipconfig[key];
      if (key == 'routingPrefix') {
        result.routingPrefix = CrOnc.getRoutingPrefixAsLength(value);
      } else {
        result[key] = value;
      }
    }
    return result;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasIpConfigFields_: function() {
    if (!this.ipConfigFields_) {
      return false;
    }
    for (let i = 0; i < this.ipConfigFields_.length; ++i) {
      if (this.get(this.ipConfigFields_[i], this.ipConfig_) != undefined) {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {string} path path to a property inside of |managedProperties|.
   * @return {string|undefined} Edit type to be used in network-property-list
   *     for the given path.
   * @private
   */
  getIPFieldEditType_: function(path) {
    if (!this.managedProperties) {
      return undefined;
    }
    const property = /** @type{!OncMojo.ManagedProperty|undefined}*/ (
        this.get(path, this.managedProperties));
    return (property && this.isNetworkPolicyEnforced(property)) ? undefined :
                                                                  'String';
  },

  /**
   * @return {Object} An object with the edit type for each editable field.
   * @private
   */
  getIPEditFields_: function() {
    if (this.automatic_ || !this.managedProperties) {
      return {};
    }
    return {
      'ipv4.ipAddress': this.getIPFieldEditType_('staticIpConfig.ipAddress'),
      'ipv4.routingPrefix':
          this.getIPFieldEditType_('staticIpConfig.routingPrefix'),
      'ipv4.gateway': this.getIPFieldEditType_('staticIpConfig.gateway')
    };
  },

  /**
   * Event triggered when the network property list changes.
   * @param {!CustomEvent<!{field: string, value: string}>} event The
   *     network-property-list change event.
   * @private
   */
  onIPChange_: function(event) {
    if (!this.ipConfig_) {
      return;
    }
    const field = event.detail.field;
    const value = event.detail.value;
    // Note: |field| includes the 'ipv4.' prefix.
    this.set('ipConfig_.' + field, value);
    this.sendStaticIpConfig_();
  },

  /** @private */
  sendStaticIpConfig_: function() {
    // This will also set IPAddressConfigType to STATIC.
    this.fire('ip-change', {
      field: 'staticIpConfig',
      value: this.ipConfig_.ipv4 ?
          this.getIPConfigProperties_(this.ipConfig_.ipv4) :
          {}
    });
  },
});
})();
