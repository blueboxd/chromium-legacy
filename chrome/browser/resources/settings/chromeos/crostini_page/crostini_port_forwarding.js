// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-port-forwarding' is the settings port forwarding subpage for
 * Crostini.
 */
Polymer({
  is: 'settings-crostini-port-forwarding',

  behaviors: [PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether Crostini is running.
     * @private {boolean}
     */
    crostiniRunning_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showAddPortDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * The forwarded ports for display in the UI.
     * @private {!Array<!CrostiniPortSetting>}
     */
    allPorts_: {
      type: Array,
      value() {
        return [];
      },
    },
  },

  /**
   * List of ports are currently being forwarded.
   * @private {!Array<?CrostiniPortActiveSetting>}
   */
  activePorts_: new Array(),

  /**
   * Tracks the last port that was selected for removal.
   * @private {?CrostiniPortActiveSetting}
   */
  lastMenuOpenedPort_: null,

  /** @override */
  attached() {
    this.addWebUIListener(
        'crostini-port-forwarder-active-ports-changed',
        this.onCrostiniPortsActiveStateChanged_.bind(this));
    this.addWebUIListener(
        'crostini-status-changed',
        this.onCrostiniIsRunningStateChanged_.bind(this));
    settings.CrostiniBrowserProxyImpl.getInstance()
        .getCrostiniActivePorts()
        .then(this.onCrostiniPortsActiveStateChanged_.bind(this));
    settings.CrostiniBrowserProxyImpl.getInstance()
        .checkCrostiniIsRunning()
        .then(this.onCrostiniIsRunningStateChanged_.bind(this));
  },

  observers:
      ['onCrostiniPortsChanged_(prefs.crostini.port_forwarding.ports.value)'],

  /**
   * @param {!CrostiniPortProtocol} protocol
   * @private
   */
  getProtocolName(protocol) {
    return Object.keys(CrostiniPortProtocol)
        .find(k => CrostiniPortProtocol[k] === protocol);
  },

  /**
   * @param {boolean} isRunning boolean indicating if Crostini is running.
   * @private
   */
  onCrostiniIsRunningStateChanged_: function(isRunning) {
    this.crostiniRunning_ = isRunning;
  },

  /**
   * @param {!Array<!CrostiniPortSetting>} ports List of ports.
   * @private
   */
  onCrostiniPortsChanged_: function(ports) {
    this.splice('allPorts_', 0, this.allPorts_.length);
    for (const port of ports) {
      port.is_active = this.activePorts_.some(
          activePort => activePort.port_number === port.port_number &&
              activePort.protocol_type === port.protocol_type);
      this.push('allPorts_', port);
    }
  },

  /**
   * @param {!Array<!CrostiniPortActiveSetting>} ports List of ports that are
   *     active.
   * @private
   */
  onCrostiniPortsActiveStateChanged_: function(ports) {
    this.activePorts_ = ports;
    for (let i = 0; i < this.allPorts_.length; i++) {
      this.set(
          `allPorts_.${i}.${'is_active'}`,
          this.activePorts_.some(
              activePort =>
                  activePort.port_number === this.allPorts_[i].port_number &&
                  activePort.protocol_type ===
                      this.allPorts_[i].protocol_type));
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAddPortClick_: function(event) {
    this.showAddPortDialog_ = true;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAddPortDialogClose_: function(event) {
    this.showAddPortDialog_ = false;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onShowRemoveAllPortsMenuClick_: function(event) {
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeAllPortsMenu.get());
    menu.showAt(/** @type {!Element} */ (event.target));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onShowRemoveSinglePortMenuClick_: function(event) {
    const dataSet = /** @type {{portNumber: string, protocolType: string}} */
        (event.currentTarget.dataset);
    this.lastMenuOpenedPort_ = {
      port_number: Number(dataSet.portNumber),
      protocol_type: /** @type {!CrostiniPortProtocol} */
          (Number(dataSet.protocolType))
    };
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeSinglePortMenu.get());
    menu.showAt(/** @type {!Element} */ (event.target));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveSinglePortClick_: function(event) {
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeSinglePortMenu.get());
    assert(
        menu.open && this.lastMenuOpenedPort_.port_number != null &&
        this.lastMenuOpenedPort_.protocol_type != null);
    settings.CrostiniBrowserProxyImpl.getInstance()
        .removeCrostiniPortForward(
            DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER,
            this.lastMenuOpenedPort_.port_number,
            this.lastMenuOpenedPort_.protocol_type)
        .then(result => {
          // TODO(crbug.com/848127): Error handling for result
          settings.recordSettingChange();
          menu.close();
        });
    this.lastMenuOpenedPort_ = null;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveAllPortsClick_: function(event) {
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeAllPortsMenu.get());
    assert(menu.open);
    settings.CrostiniBrowserProxyImpl.getInstance()
        .removeAllCrostiniPortForwards(
            DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER);
    settings.recordSettingChange();
    menu.close();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onPortActivationChange_: function(event) {
    const dataSet = /** @type {{portNumber: string, protocolType: string}} */
        (event.currentTarget.dataset);
    const portNumber = Number(dataSet.portNumber);
    const protocolType = /** @type {!CrostiniPortProtocol} */
        (Number(dataSet.protocolType));
    if (event.target.checked) {
      event.target.checked = false;
      settings.CrostiniBrowserProxyImpl.getInstance()
          .activateCrostiniPortForward(
              DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER, portNumber,
              protocolType)
          .then(
              result => {
                  // TODO(crbug.com/848127): Error handling for result
              });
    } else {
      settings.CrostiniBrowserProxyImpl.getInstance()
          .deactivateCrostiniPortForward(
              DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER, portNumber,
              protocolType)
          .then(
              result => {
                  // TODO(crbug.com/848127): Error handling for result
              });
    }
  },
});
