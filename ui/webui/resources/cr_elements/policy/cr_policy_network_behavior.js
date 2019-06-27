// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled network properties.
 */

/** @polymerBehavior */
const CrPolicyNetworkBehavior = {
  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by a policy
   *     (either enforced or recommended).
   */
  isNetworkPolicyControlled: function(property) {
    // If the property is not a dictionary, or does not have an Effective
    // sub-property set, then the property is not policy controlled.
    if (typeof property != 'object' || !property.Effective) {
      return false;
    }
    // Enforced
    const effective = property.Effective;
    if (effective == 'UserPolicy' || effective == 'DevicePolicy') {
      return true;
    }
    // Recommended
    if (typeof property.UserPolicy != 'undefined' ||
        typeof property.DevicePolicy != 'undefined') {
      return true;
    }
    // Neither enforced nor recommended = not policy controlled.
    return false;
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by an
   *     extension.
   */
  isExtensionControlled: function(property) {
    return typeof property == 'object' &&
        property.Effective == 'ActiveExtension';
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by a policy
   *     or an extension.
   */
  isControlled: function(property) {
    return this.isNetworkPolicyControlled(property) ||
        this.isExtensionControlled(property);
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is editable.
   */
  isEditable: function(property) {
    // If the property is not a dictionary, then the property is not editable.
    if (typeof property != 'object') {
      return false;
    }

    // If the property has a UserEditable sub-property, that determines whether
    // or not it is editable.
    if (typeof property.UserEditable != 'undefined') {
      return property.UserEditable;
    }

    // Otherwise if the property has a DeviceEditable sub-property, check that.
    if (typeof property.DeviceEditable != 'undefined') {
      return property.DeviceEditable;
    }

    // If no 'Editable' sub-property exists, the policy value is not editable.
    return false;
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is enforced by a policy.
   */
  isNetworkPolicyEnforced: function(property) {
    return this.isNetworkPolicyControlled(property) &&
        !this.isEditable(property);
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is recommended by a policy.
   */
  isNetworkPolicyRecommended: function(property) {
    return this.isNetworkPolicyControlled(property) &&
        this.isEditable(property);
  },

  /**
   * @param {string|undefined} source
   * @return {boolean}
   * @protected
   */
  isPolicySource: function(source) {
    return !!source &&
        (source == CrOnc.Source.DEVICE_POLICY ||
         source == CrOnc.Source.USER_POLICY);
  },

  /**
   * @param {chromeos.networkConfig.mojom.OncSource} source
   * @return {boolean}
   * @protected
   */
  isPolicySourceMojo: function(source) {
    return !!source &&
        (source == chromeos.networkConfig.mojom.OncSource.kDevicePolicy ||
         source == chromeos.networkConfig.mojom.OncSource.kUserPolicy);
  },

  /**
   * @param {string} source
   * @return {!CrPolicyIndicatorType}
   * @private
   */
  getIndicatorTypeForSource: function(source) {
    if (source == CrOnc.Source.DEVICE_POLICY) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }
    if (source == CrOnc.Source.USER_POLICY) {
      return CrPolicyIndicatorType.USER_POLICY;
    }
    return CrPolicyIndicatorType.NONE;
  },

  /**
   * @param {chromeos.networkConfig.mojom.OncSource} source
   * @return {!CrPolicyIndicatorType}
   * @private
   */
  getIndicatorTypeForSourceMojo: function(source) {
    if (source == chromeos.networkConfig.mojom.OncSource.kDevicePolicy) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }
    if (source == chromeos.networkConfig.mojom.OncSource.kUserPolicy) {
      return CrPolicyIndicatorType.USER_POLICY;
    }
    return CrPolicyIndicatorType.NONE;
  },

  /**
   * @param {Object} dict A managed ONC dictionary.
   * @param {string} path A path to a setting inside |dict|.
   * @return {!CrOnc.ManagedProperty|undefined} The value of the setting at
   * |path|.
   * @private
   */
  getSettingAtPath_: function(dict, path) {
    const keys = path.split('.');
    for (let i = 0; i < keys.length; ++i) {
      const key = keys[i];
      if (typeof dict !== 'object' || !(key in dict)) {
        return undefined;
      }
      dict = dict[key];
    }
    return dict;
  },

  /**
   * Get managed property at the given path. If the property is not policy
   * managed, return 'undefined'. If the property's value is 'undefined' return
   * a non-editable policy managed 'CrOnc.ManagedProperty' object.
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {string} path
   * @return {!CrOnc.ManagedProperty|undefined}
   * @private
   */
  getManagedSettingAtPath_: function(networkProperties, path) {
    if (!this.isPolicySource(networkProperties.Source)) {
      return undefined;
    }
    const setting = this.getSettingAtPath_(networkProperties, path);
    if (setting) {
      return setting;
    }
    // If setting is not defined, return a non-editable managed property with
    // 'undefined' value enforced by 'networkProperties.Source'.
    return {
      'Effective': networkProperties.Source,
      'DeviceEditable': false,
      'UserEditable': false,
      'UserPolicy': undefined,
      'DevicePolicy': undefined
    };
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {string} path A path to a setting inside |networkProperties|.
   * @return {boolean} True if the setting at |path| is managed by policy (e.g.
   * 'StaticIPConfig.NameServers').
   */
  isNetworkPolicyPathManaged: function(networkProperties, path) {
    return this.isNetworkPolicyControlled(
        this.getManagedSettingAtPath_(networkProperties, path));
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @param {string} path A path to a setting inside |networkProperties|.
   * @return {boolean} True if the setting at |path| is enforced by policy (e.g.
   * 'StaticIPConfig.NameServers').
   */
  isNetworkPolicyPathEnforced: function(networkProperties, path) {
    return this.isNetworkPolicyEnforced(
        this.getManagedSettingAtPath_(networkProperties, path));
  },

  /**
   * Get policy indicator type for the setting at |path|.
   * @param {CrOnc.NetworkProperties} networkProperties
   * @param {string} path
   * @return {CrPolicyIndicatorType}
   */
  getPolicyIndicatorType_: function(networkProperties, path) {
    if (!this.isNetworkPolicyPathManaged(networkProperties, path)) {
      return CrPolicyIndicatorType.NONE;
    }
    return networkProperties.Source == CrOnc.Source.DEVICE_POLICY ?
        CrPolicyIndicatorType.DEVICE_POLICY :
        CrPolicyIndicatorType.USER_POLICY;
  },
};
