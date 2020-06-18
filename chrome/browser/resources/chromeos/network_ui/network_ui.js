// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Builds UI elements shown in chrome://networks debugging page.
 */

/**
 * @typedef {!OncMojo.DeviceStateProperties|!OncMojo.NetworkStateProperties}
 */
OncMojo.StateProperties;

cr.define('network_ui', function() {
  'use strict';

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let networkConfig = null;

  /**
   * Callback invoked by Chrome after a openCellularActivationUi call.
   * @param {boolean} didOpenActivationUi Whether the activation UI was actually
   *     opened. If this value is false, it means that no cellular network was
   *     available to be activated.
   */
  const openCellularActivationUiResult = function(didOpenActivationUi) {
    $('cellular-error-text').hidden = didOpenActivationUi;
  };

  /**
   * Requests that the cellular activation UI be displayed.
   */
  const openCellularActivationUi = function() {
    chrome.send('openCellularActivationUi');
  };

  /**
   * Requests that the "add Wi-Fi network" UI be displayed.
   */
  const showAddNewWifi = function() {
    chrome.send('showAddNewWifi');
  };

  /**
   * Requests the global policy dictionary and updates the page.
   */
  const requestGlobalPolicy = function() {
    networkConfig.getGlobalPolicy().then(result => {
      document.querySelector('#global-policy').textContent =
          JSON.stringify(result.result, null, '\t');
    });
  };

  /** Initialize NetworkUI state. */
  const init = function() {
    networkConfig = network_config.MojoInterfaceProviderImpl.getInstance()
                        .getMojoServiceRemote();
  };

  /**
   * Handles clicks on network items in the <network-select> element by
   * attempting a connection to the selected network or requesting a password
   * if the network requires a password.
   * @param {!Event<!OncMojo.NetworkStateProperties>} event
   */
  const onNetworkItemSelected = function(event) {
    const networkState = event.detail;

    // If the network is already connected, show network details.
    if (OncMojo.connectionStateIsConnected(networkState.connectionState)) {
      chrome.send('showNetworkDetails', [networkState.guid]);
      return;
    }

    // If the network is not connectable, show a configuration dialog.
    if (networkState.connectable === false || networkState.errorState) {
      chrome.send('showNetworkConfig', [networkState.guid]);
      return;
    }

    // Otherwise, connect.
    networkConfig.startConnect(networkState.guid).then(response => {
      if (response.result ==
          chromeos.networkConfig.mojom.StartConnectResult.kSuccess) {
        return;
      }
      console.error(
          'startConnect error for: ' + networkState.guid + ' Result: ' +
          response.result.toString() + ' Message: ' + response.message);
      chrome.send('showNetworkConfig', [networkState.guid]);
    });
  };

  const selectTabFromHash = function() {
    const selectedTab = window.location.hash.substring(1);
    if (!selectedTab)
      return;
    const tabpanel = document.querySelector('tabpanels > #' + selectedTab);
    if (tabpanel) {
      tabpanel.selected = true;
    }
  };

  const getShillNetworkPropertiesResult = function(args) {
    document.querySelector('network-state-ui')
        .getShillNetworkPropertiesResult(args);
  };
  const getShillDevicePropertiesResult = function(args) {
    document.querySelector('network-state-ui')
        .getShillDevicePropertiesResult(args);
  };
  const getShillEthernetEAPResult = function(args) {
    document.querySelector('network-state-ui').getShillEthernetEAPResult(args);
  };

  function onLoad() {
    cr.ui.decorate('tabbox', cr.ui.TabBox);
    const select = document.querySelector('network-select');
    select.customItems = [
      {customItemName: 'Add WiFi', polymerIcon: 'cr:add', customData: 'WiFi'},
      {customItemName: 'Add VPN', polymerIcon: 'cr:add', customData: 'VPN'}
    ];
    select.addEventListener('network-item-selected', onNetworkItemSelected);
    $('cellular-activation-button').onclick = openCellularActivationUi;
    $('add-new-wifi-button').onclick = showAddNewWifi;

    document.addEventListener('custom-item-selected', function(event) {
      chrome.send('addNetwork', [event.detail.customData]);
    });

    window.addEventListener('hashchange', function(event) {
      selectTabFromHash();
    });
    selectTabFromHash();

    init();
    requestGlobalPolicy();
  }

  return {
    getShillNetworkPropertiesResult: getShillNetworkPropertiesResult,
    getShillDevicePropertiesResult: getShillDevicePropertiesResult,
    getShillEthernetEAPResult: getShillEthernetEAPResult,
    openCellularActivationUiResult: openCellularActivationUiResult,
    onLoad: onLoad
  };
});

document.addEventListener('DOMContentLoaded', network_ui.onLoad);
