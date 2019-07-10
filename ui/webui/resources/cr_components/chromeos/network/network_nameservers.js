// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network nameserver options.
 */
Polymer({
  is: 'network-nameservers',

  behaviors: [I18nBehavior, CrPolicyNetworkBehavior],

  properties: {
    /**
     * The network properties dictionary containing the nameserver properties to
     * display and modify.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_',
    },

    /**
     * Array of nameserver addresses stored as strings.
     * @private {!Array<string>}
     */
    nameservers_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The selected nameserver type.
     * @private
     */
    nameserversType_: {
      type: String,
      value: 'automatic',
    },

    /** @private */
    googleNameserversText_: {
      type: String,
      value: function() {
        return this.i18nAdvanced(
            'networkNameserversGoogle', {substitutions: [], tags: ['a']});
      }
    },

    /** @private */
    canChangeConfigType_: {
      type: Boolean,
      computed: 'computeCanChangeConfigType_(networkProperties)',
    }
  },

  /** @const */
  GOOGLE_NAMESERVERS: [
    '8.8.4.4',
    '8.8.8.8',
  ],

  /** @const */
  EMPTY_NAMESERVER: '0.0.0.0',

  /** @const */
  MAX_NAMESERVERS: 4,

  /**
   * Saved nameservers when switching to 'automatic'.
   * @private {!Array<string>}
   */
  savedNameservers_: [],

  /**
   * Returns true if |nameservers| contains any all google nameserver entries
   * and only google nameserver entries or empty entries.
   * @param {!Array<string>} nameservers
   * @private
   */
  isGoogleNameservers_: function(nameservers) {
    const matches = [];
    for (let i = 0; i < nameservers.length; ++i) {
      const nameserver = nameservers[i];
      if (nameserver == this.EMPTY_NAMESERVER) {
        continue;
      }
      let valid = false;
      for (let j = 0; j < this.GOOGLE_NAMESERVERS.length; ++j) {
        if (nameserver == this.GOOGLE_NAMESERVERS[j]) {
          valid = true;
          matches[j] = true;
          break;
        }
      }
      if (!valid) {
        return false;
      }
    }
    for (let j = 0; j < this.GOOGLE_NAMESERVERS.length; ++j) {
      if (!matches[j]) {
        return false;
      }
    }
    return true;
  },

  /** @private */
  networkPropertiesChanged_: function(newValue, oldValue) {
    if (!this.networkProperties) {
      return;
    }

    if (!oldValue || newValue.GUID != oldValue.GUID) {
      this.savedNameservers_ = [];
    }

    // Update the 'nameservers' property.
    let nameservers = [];
    const ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    if (ipv4 && ipv4.NameServers) {
      nameservers = ipv4.NameServers;
    }

    // Update the 'nameserversType' property.
    const configType =
        CrOnc.getActiveValue(this.networkProperties.NameServersConfigType);
    let type;
    if (configType == CrOnc.IPConfigType.STATIC) {
      if (this.isGoogleNameservers_(nameservers)) {
        type = 'google';
        nameservers = this.GOOGLE_NAMESERVERS;  // Use consistent order.
      } else {
        type = 'custom';
      }
    } else {
      type = 'automatic';
      nameservers = this.clearEmptyNameServers_(nameservers);
    }
    this.setNameservers_(type, nameservers, false /* send */);
  },

  /**
   * @param {string} nameserversType
   * @param {!Array<string>} nameservers
   * @param {boolean} sendNameservers If true, send the nameservers once they
   *     have been set in the UI.
   * @private
   */
  setNameservers_: function(nameserversType, nameservers, sendNameservers) {
    if (nameserversType == 'custom') {
      // Add empty entries for unset custom nameservers.
      for (let i = nameservers.length; i < this.MAX_NAMESERVERS; ++i) {
        nameservers[i] = this.EMPTY_NAMESERVER;
      }
      if (!this.isGoogleNameservers_(nameservers)) {
        this.savedNameservers_ = nameservers.slice();
      }
    }
    this.nameservers_ = nameservers;
    // Set nameserversType_ after dom-repeat has been stamped.
    this.async(() => {
      this.nameserversType_ = nameserversType;
      if (sendNameservers) {
        this.sendNameServers_();
      }
    });
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean} True if the nameservers config type type can be changed.
   * @private
   */
  computeCanChangeConfigType_: function(networkProperties) {
    return !this.isNetworkPolicyPathEnforced(
               networkProperties, 'NameServersConfigType') &&
        !this.isNetworkPolicyPathEnforced(
            networkProperties, 'StaticIPConfig.NameServers');
  },

  /**
   * @param {string} nameserversType
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean} True if the nameservers are editable.
   * @private
   */
  canEditCustomNameServers_: function(nameserversType, networkProperties) {
    return nameserversType == 'custom' &&
        !this.isNetworkPolicyEnforced(
            networkProperties.NameServersConfigType) &&
        (!networkProperties.StaticIPConfig ||
         !this.isNetworkPolicyEnforced(
             networkProperties.StaticIPConfig.NameServers));
  },

  /**
   * @param {string} nameserversType
   * @param {string} type
   * @param {!Array<string>} nameservers
   * @return {boolean}
   * @private
   */
  showNameservers_: function(nameserversType, type, nameservers) {
    if (nameserversType != type) {
      return false;
    }
    return type == 'custom' || nameservers.length > 0;
  },

  /**
   * @param {!Array<string>} nameservers
   * @return {string}
   * @private
   */
  getNameserversString_: function(nameservers) {
    return nameservers.join(', ');
  },

  /**
   * Event triggered when the selected type changes. Updates nameservers and
   * sends the change value if necessary.
   * @private
   */
  onTypeChange_: function() {
    const type = this.$$('#nameserverType').selected;
    this.nameserversType_ = type;
    if (type == 'custom') {
      // Restore the saved nameservers.
      this.setNameservers_(type, this.savedNameservers_, true /* send */);
      return;
    }
    this.sendNameServers_();
  },

  /**
   * Event triggered when a nameserver value changes.
   * @private
   */
  onValueChange_: function() {
    if (this.nameserversType_ != 'custom') {
      // If a user inputs Google nameservers in the custom nameservers fields,
      // |nameserversType| will change to 'google' so don't send the values.
      return;
    }
    this.sendNameServers_();
  },

  /**
   * Sends the current nameservers type (for automatic) or value.
   * @private
   */
  sendNameServers_: function() {
    const type = this.nameserversType_;

    if (type == 'custom') {
      const nameservers = new Array(this.MAX_NAMESERVERS);
      for (let i = 0; i < this.MAX_NAMESERVERS; ++i) {
        const nameserverInput = this.$$('#nameserver' + i);
        nameservers[i] = nameserverInput ? nameserverInput.value : '';
      }
      this.nameservers_ = nameservers;
      this.savedNameservers_ = nameservers.slice();
      this.fire('nameservers-change', {
        field: 'NameServers',
        value: nameservers,
      });
    } else if (type == 'google') {
      this.nameservers_ = this.GOOGLE_NAMESERVERS;
      this.fire('nameservers-change', {
        field: 'NameServers',
        value: this.GOOGLE_NAMESERVERS,
      });
    } else {  // type == automatic
      // If not connected, properties will clear. Otherwise they may or may not
      // change so leave them as-is.
      if (this.networkProperties.ConnectionState !=
          CrOnc.ConnectionState.CONNECTED) {
        this.nameservers_ = [];
      } else {
        this.nameservers_ = this.clearEmptyNameServers_(this.nameservers_);
      }
      this.fire('nameservers-change', {
        field: 'NameServersConfigType',
        value: CrOnc.IPConfigType.DHCP,
      });
    }
  },

  /**
   * @param {!Array<string>} nameservers
   * @return {!Array<string>}
   * @private
   */
  clearEmptyNameServers_: function(nameservers) {
    return nameservers.filter((nameserver) => !!nameserver);
  },

  /**
   * @param {!Event} event
   * @private
   */
  doNothing_: function(event) {
    event.stopPropagation();
  },
});
