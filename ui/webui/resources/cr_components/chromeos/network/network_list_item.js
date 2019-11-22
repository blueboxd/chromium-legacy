// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list based on ONC state properties.
 */

Polymer({
  is: 'network-list-item',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    I18nBehavior,
  ],

  properties: {
    /** @type {!NetworkList.NetworkListItemType|undefined} */
    item: {
      type: Object,
      observer: 'itemChanged_',
    },

    /**
     * The ONC data properties used to display the list item.
     * @type {!OncMojo.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object,
      observer: 'networkStateChanged_',
    },

    /** Whether to show any buttons for network items. Defaults to false. */
    showButtons: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * Reflect the element's tabindex attribute to a property so that embedded
     * elements (e.g. the show subpage button) can become keyboard focusable
     * when this element has keyboard focus.
     */
    tabindex: {
      type: Number,
      value: -1,
      reflectToAttribute: true,
    },

    /**
     * Expose the itemName so it can be used as a label for a11y.  It will be
     * added as an attribute on this top-level network-list-item, and can
     * be used by any sub-element which applies it.
     */
    ariaLabel: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getAriaLabel_(item, networkState)',
    },

    buttonLabel: {
      type: String,
      computed: 'getButtonLabel_(item)',
    },

    /** Expose the aria role attribute as "button". */
    role: {
      type: String,
      reflectToAttribute: true,
      value: 'button',
    },

    /**
     * The cached ConnectionState for the network.
     * @type {!chromeos.networkConfig.mojom.ConnectionStateType|undefined}
     */
    connectionState_: Number,

    /** Whether to show technology badge on mobile network icon. */
    showTechnologyBadge: {
      type: Boolean,
      value: true,
    },

    /** Whether cellular activation is unavailable in the current context. */
    activationUnavailable: {
      type: Boolean,
      value: false,
    }
  },

  /** @override */
  attached: function() {
    this.listen(this, 'keydown', 'onKeydown_');
  },

  /** @override */
  detached: function() {
    this.unlisten(this, 'keydown', 'onKeydown_');
  },

  /** @private */
  itemChanged_: function() {
    if (this.item && !this.item.hasOwnProperty('customItemName')) {
      this.networkState =
          /** @type {!OncMojo.NetworkStateProperties} */ (this.item);
    } else if (this.networkState) {
      this.networkState = undefined;
    }
  },

  /** @private */
  networkStateChanged_: function() {
    if (!this.networkState) {
      return;
    }
    const connectionState = this.networkState.connectionState;
    if (connectionState == this.connectionState_) {
      return;
    }
    this.connectionState_ = connectionState;
    this.fire('network-connect-changed', this.networkState);
  },

  /**
   * This gets called for network items and custom items.
   * @return {string}
   * @private
   */
  getItemName_: function() {
    if (this.item.hasOwnProperty('customItemName')) {
      const item = /** @type {!NetworkList.CustomItemState} */ (this.item);
      const name = item.customItemName || '';
      const customName = this.i18n(item.customItemName);

      return customName ? customName : name;
    }
    return OncMojo.getNetworkStateDisplayName(
        /** @type {!OncMojo.NetworkStateProperties} */ (this.item));
  },

  /**
   * The aria label for the subpage button.
   * @return {string}
   * @private
   */
  getButtonLabel_: function() {
    return this.i18n('networkListItemSubpageButtonLabel', this.getItemName_());
  },

  /**
   * Label for the row, used for accessibility announcement.
   * @return {string}
   * @private
   */
  getAriaLabel_: function() {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    const OncSource = chromeos.networkConfig.mojom.OncSource;
    const SecurityType = chromeos.networkConfig.mojom.SecurityType;
    const status = this.getNetworkStateText_();
    const isManaged = this.item.source === OncSource.kDevicePolicy ||
        this.item.source === OncSource.kUserPolicy;
    const index = this.parentElement.items.indexOf(this.item) + 1;
    const total = this.parentElement.items.length;
    switch (this.item.type) {
      case NetworkType.kCellular:
        if (isManaged) {
          if (status) {
            return this.i18n(
                'networkListItemLabelCellularManagedWithConnectionStatus',
                index, total, this.getItemName_(), status,
                this.item.typeState.cellular.signalStrength);
          }
          return this.i18n(
              'networkListItemLabelCellularManaged', index, total,
              this.getItemName_(), this.item.typeState.cellular.signalStrength);
        }
        if (status) {
          return this.i18n(
              'networkListItemLabelCellularWithConnectionStatus', index, total,
              this.getItemName_(), status,
              this.item.typeState.cellular.signalStrength);
        }
        return this.i18n(
            'networkListItemLabelCellular', index, total, this.getItemName_(),
            this.item.typeState.cellular.signalStrength);
      case NetworkType.kEthernet:
        if (isManaged) {
          if (status) {
            return this.i18n(
                'networkListItemLabelCellularManagedWithConnectionStatus',
                index, total, this.getItemName_(), status);
          }
          return this.i18n(
              'networkListItemLabelEthernetManaged', index, total,
              this.getItemName_());
        }
        if (status) {
          return this.i18n(
              'networkListItemLabelEthernetWithConnectionStatus', index, total,
              this.getItemName_(), status);
        }
        return this.i18n(
            'networkListItemLabel', index, total, this.getItemName_());
      case NetworkType.kTether:
        // Tether networks will never be controlled by policy (only disabled).
        if (status) {
          return this.i18n(
              'networkListItemLabelTetherWithConnectionStatus', index, total,
              this.getItemName_(), status,
              this.item.typeState.tether.signalStrength,
              this.item.typeState.tether.batteryPercentage);
        }
        return this.i18n(
            'networkListItemLabelTether', index, total, this.getItemName_(),
            this.item.typeState.tether.signalStrength,
            this.item.typeState.tether.batteryPercentage);
      case NetworkType.kWiFi:
        const secured =
            this.item.typeState.wifi.security === SecurityType.kNone ?
            this.i18n('wifiNetworkStatusUnsecured') :
            this.i18n('wifiNetworkStatusSecured');
        if (isManaged) {
          if (status) {
            return this.i18n(
                'networkListItemLabelWifiManagedWithConnectionStatus', index,
                total, this.getItemName_(), secured, status,
                this.item.typeState.wifi.signalStrength);
          }
          return this.i18n(
              'networkListItemLabelWifiManaged', index, total,
              this.getItemName_(), secured,
              this.item.typeState.wifi.signalStrength);
        }
        if (status) {
          return this.i18n(
              'networkListItemLabelWifiWithConnectionStatus', index, total,
              this.getItemName_(), secured, status,
              this.item.typeState.wifi.signalStrength);
        }
        return this.i18n(
            'networkListItemLabelWifi', index, total, this.getItemName_(),
            secured, this.item.typeState.wifi.signalStrength);
      default:
        return this.i18n(
            'networkListItemLabel', index, total, this.getItemName_());
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  isStateTextVisible_: function() {
    return !!this.networkState && !!this.getNetworkStateText_();
  },

  /**
   * This only gets called for network items once networkState is set.
   * @return {string}
   * @private
   */
  getNetworkStateText_: function() {
    const mojom = chromeos.networkConfig.mojom;
    if (!this.networkState) {
      return '';
    }
    const connectionState = this.networkState.connectionState;
    if (this.networkState.type == mojom.NetworkType.kCellular) {
      if (this.shouldShowNotAvailableText_()) {
        return this.i18n('networkListItemNotAvailable');
      }
      if (this.networkState.typeState.cellular.scanning) {
        return this.i18n('networkListItemScanning');
      }
      if (this.networkState.typeState.cellular.simLocked) {
        return this.i18n('networkListItemSimCardLocked');
      }
    }
    if (OncMojo.connectionStateIsConnected(connectionState)) {
      // TODO(khorimoto): Consider differentiating between Portal, Connected,
      // and Online.
      return this.i18n('networkListItemConnected');
    }
    if (connectionState == mojom.ConnectionStateType.kConnecting) {
      return this.i18n('networkListItemConnecting');
    }
    return '';
  },

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} networkState
   * @param {boolean} showButtons
   * @return {boolean}
   * @private
   */
  isSubpageButtonVisible_: function(networkState, showButtons) {
    return !!networkState && showButtons;
  },

  /**
   * @return {boolean} Whether this element's contents describe an "active"
   *     network. In this case, an active network is connected and may have
   *     additional properties (e.g., must be activated for cellular networks).
   * @private
   */
  isStateTextActive_: function() {
    if (!this.networkState) {
      return false;
    }
    if (this.shouldShowNotAvailableText_()) {
      return false;
    }
    return OncMojo.connectionStateIsConnected(
        this.networkState.connectionState);
  },

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_: function(event) {
    // The only key event handled by this element is pressing Enter when the
    // subpage arrow is focused.
    if (event.key != 'Enter' ||
        !this.isSubpageButtonVisible_(this.networkState, this.showButtons) ||
        this.$$('#subpage-button') != this.shadowRoot.activeElement) {
      return;
    }

    this.fireShowDetails_(event);

    // The default event for pressing Enter on a focused button is to simulate a
    // click on the button. Prevent this action, since it would navigate a
    // second time to the details page and cause an unnecessary entry to be
    // added to the back stack. See https://crbug.com/736963.
    event.preventDefault();
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onSubpageArrowClick_: function(event) {
    this.fireShowDetails_(event);
  },

  /**
   * Fires a 'show-details' event with |this.networkState| as the details.
   * @param {!Event} event
   * @private
   */
  fireShowDetails_: function(event) {
    assert(this.networkState);
    this.fire('show-detail', this.networkState);
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNotAvailableText_: function() {
    if (!this.networkState || !this.activationUnavailable) {
      return false;
    }

    // If cellular activation is not currently available and |this.networkState|
    // describes an unactivated cellular network, the text should be shown.
    const mojom = chromeos.networkConfig.mojom;
    return this.networkState.type == mojom.NetworkType.kCellular &&
        this.networkState.typeState.cellular.activationState !=
        mojom.ActivationStateType.kActivated;
  },
});
