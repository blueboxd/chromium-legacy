// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Builds UI elements shown in chrome://networks debugging page.
 */
const networkUI = {};

/** @typedef {chromeos.networkConfig.mojom.NetworkStateProperties} */
networkUI.NetworkStateProperties;

/** @typedef {chromeos.networkConfig.mojom.DeviceStateProperties} */
networkUI.DeviceStateProperties;

/**
 * @typedef {networkUI.NetworkStateProperties|networkUI.DeviceStateProperties}
 */
networkUI.StateProperties;

const NetworkUI = (function() {
  'use strict';

  const mojom = chromeos.networkConfig.mojom;

  CrOncStrings = {
    OncTypeCellular: loadTimeData.getString('OncTypeCellular'),
    OncTypeEthernet: loadTimeData.getString('OncTypeEthernet'),
    OncTypeMobile: loadTimeData.getString('OncTypeMobile'),
    OncTypeTether: loadTimeData.getString('OncTypeTether'),
    OncTypeVPN: loadTimeData.getString('OncTypeVPN'),
    OncTypeWiFi: loadTimeData.getString('OncTypeWiFi'),
    OncTypeWiMAX: loadTimeData.getString('OncTypeWiMAX'),
    networkListItemConnected:
        loadTimeData.getString('networkListItemConnected'),
    networkListItemConnecting:
        loadTimeData.getString('networkListItemConnecting'),
    networkListItemConnectingTo:
        loadTimeData.getString('networkListItemConnectingTo'),
    networkListItemInitializing:
        loadTimeData.getString('networkListItemInitializing'),
    networkListItemScanning: loadTimeData.getString('networkListItemScanning'),
    networkListItemNotConnected:
        loadTimeData.getString('networkListItemNotConnected'),
    networkListItemNoNetwork:
        loadTimeData.getString('networkListItemNoNetwork'),
    vpnNameTemplate: loadTimeData.getString('vpnNameTemplate'),
  };

  // Properties to display in the network state table. Each entry can be either
  // a single state field or an array of state fields. If more than one is
  // specified then the first non empty value is used.
  const NETWORK_STATE_FIELDS = [
    'guid', 'name', 'type', 'connectionState', 'connectable', 'errorState',
    'wifi.security', ['cellular.networkTechnology', 'EAP.EAP'],
    'cellular.activationState', 'cellular.roaming', 'wifi.frequency',
    'wifi.signalStrength'
  ];

  const FAVORITE_STATE_FIELDS = ['guid', 'name', 'type', 'source'];

  const DEVICE_STATE_FIELDS = ['type', 'deviceState'];

  /**
   * This UI will use both the networkingPrivate extension API and the
   * networkConfig mojo API until we provide all of the required functionality
   * in networkConfig. TODO(stevenjb): Remove use of networkingPrivate api.
   * @type {?mojom.CrosNetworkConfigProxy}
   */
  let networkConfigProxy = null;

  /**
   * Creates and returns a typed HTMLTableCellElement.
   *
   * @return {!HTMLTableCellElement} A new td element.
   */
  const createTableCellElement = function() {
    return /** @type {!HTMLTableCellElement} */ (document.createElement('td'));
  };

  /**
   * Creates and returns a typed HTMLTableRowElement.
   *
   * @return {!HTMLTableRowElement} A new tr element.
   */
  const createTableRowElement = function() {
    return /** @type {!HTMLTableRowElement} */ (document.createElement('tr'));
  };

  /**
   * Returns the ONC data property for |state| associated with a key. Used
   * to access properties in the state by |key| which may may refer to a
   * nested property, e.g. 'WiFi.Security'. If any part of a nested key is
   * missing, this will return undefined.
   *
   * @param {!networkUI.StateProperties} state
   * @param {string} key The ONC key for the property.
   * @return {*} The value associated with the property or undefined if the
   *     key (any part of it) is not defined.
   */
  const getOncProperty = function(state, key) {
    let dict = /** @type {!Object} */ (state);
    const keys = key.split('.');
    while (keys.length > 1) {
      const k = keys.shift();
      dict = dict[k];
      if (!dict || typeof dict != 'object')
        return undefined;
    }
    const k = keys.shift();
    return OncMojo.getTypeString(k, dict[k]);
  };

  /**
   * Creates a cell with a button for expanding a network state table row.
   *
   * @param {!networkUI.StateProperties} state
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     given value.
   */
  const createStateTableExpandButton = function(state) {
    const cell = createTableCellElement();
    cell.className = 'state-table-expand-button-cell';
    const button = document.createElement('button');
    button.addEventListener('click', function(event) {
      toggleExpandRow(/** @type {!HTMLElement} */ (event.target), state);
    });
    button.className = 'state-table-expand-button';
    button.textContent = '+';
    cell.appendChild(button);
    return cell;
  };

  /**
   * Creates a cell with an icon representing the network state.
   *
   * @param {!networkUI.StateProperties} state
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     icon.
   */
  const createStateTableIcon = function(state) {
    const cell = createTableCellElement();
    cell.className = 'state-table-icon-cell';
    const icon = /** @type {!CrNetworkIconElement} */ (
        document.createElement('cr-network-icon'));
    icon.isListItem = true;
    icon.networkState = OncMojo.getDefaultNetworkState(state.type);
    cell.appendChild(icon);
    return cell;
  };

  /**
   * Creates a cell in the network state table.
   *
   * @param {*} value Content in the cell.
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     given value.
   */
  const createStateTableCell = function(value) {
    const cell = createTableCellElement();
    cell.textContent = value || '';
    return cell;
  };

  /**
   * Creates a row in the network state table.
   *
   * @param {Array} stateFields The state fields to use for the row.
   * @param {!networkUI.StateProperties} state
   * @return {!HTMLTableRowElement} The created tr element that contains the
   *     network state information.
   */
  const createStateTableRow = function(stateFields, state) {
    const row = createTableRowElement();
    row.className = 'state-table-row';
    row.appendChild(createStateTableExpandButton(state));
    row.appendChild(createStateTableIcon(state));
    for (let i = 0; i < stateFields.length; ++i) {
      const field = stateFields[i];
      let value;
      if (typeof field == 'string') {
        value = getOncProperty(state, field);
      } else {
        for (let j = 0; j < field.length; ++j) {
          value = getOncProperty(state, field[j]);
          if (value != undefined)
            break;
        }
      }
      if (field == 'guid')
        value = value.slice(0, 8);
      row.appendChild(createStateTableCell(value));
    }
    return row;
  };

  /**
   * Creates a table for networks or favorites.
   *
   * @param {string} tablename The name of the table to be created.
   * @param {!Array<string>} stateFields The list of fields for the table.
   * @param {!Array<!networkUI.StateProperties>} states
   */
  const createStateTable = function(tablename, stateFields, states) {
    const table = $(tablename);
    const oldRows = table.querySelectorAll('.state-table-row');
    for (let i = 0; i < oldRows.length; ++i)
      table.removeChild(oldRows[i]);
    states.forEach(function(state) {
      table.appendChild(createStateTableRow(stateFields, state));
    });
  };

  /**
   * Returns a valid HTMLElement id from |guid|.
   *
   * @param {string} guid A GUID which may start with a digit.
   * @return {string} A valid HTMLElement id.
   */
  const idFromGuid = function(guid) {
    return '_' + guid.replace(/[{}]/g, '');
  };

  /**
   * Returns a valid HTMLElement id from |type|. Note: |type| may be a Shill
   * type or an ONC type, so strip _ and convert to lowercase to unify them.
   *
   * @param {string} type A Shill or ONC network type
   * @return {string} A valid HTMLElement id.
   */
  const idFromTypeString = function(type) {
    return '_' + type.replace(/[{}_]/g, '').toLowerCase();
  };

  /**
   * @param {!mojom.NetworkType} type
   * @return {string} A valid HTMLElement id.
   */
  const idFromType = function(type) {
    return idFromTypeString(OncMojo.getNetworkTypeString(type));
  };

  /**
   * This callback function is triggered when visible networks are received.
   *
   * @param {!Array<!networkUI.NetworkStateProperties>} states
   */
  const onVisibleNetworksReceived = function(states) {
    createStateTable('network-state-table', NETWORK_STATE_FIELDS, states);
  };

  /**
   * This callback function is triggered when favorite networks are received.
   *
   * @param {!Array<!networkUI.NetworkStateProperties>} states
   */
  const onFavoriteNetworksReceived = function(states) {
    createStateTable('favorite-state-table', FAVORITE_STATE_FIELDS, states);
  };

  /**
   * This callback function is triggered when device states are received.
   *
   * @param {!Array<!networkUI.DeviceStateProperties>} states
   */
  const onDeviceStatesReceived = function(states) {
    createStateTable('device-state-table', DEVICE_STATE_FIELDS, states);
  };

  /**
   * Toggles the button state and add or remove a row displaying the complete
   * state information for a row.
   *
   * @param {!HTMLElement} btn The button that was clicked.
   * @param {!networkUI.StateProperties} state
   */
  const toggleExpandRow = function(btn, state) {
    const cell = btn.parentNode;
    const row = /** @type {!HTMLTableRowElement} */ (cell.parentNode);
    if (btn.textContent == '-') {
      btn.textContent = '+';
      row.parentNode.removeChild(row.nextSibling);
    } else {
      btn.textContent = '-';
      const expandedRow = createExpandedRow(state, row);
      row.parentNode.insertBefore(expandedRow, row.nextSibling);
    }
  };

  /**
   * Creates the expanded row for displaying the complete state as JSON.
   *
   * @param {!networkUI.StateProperties} state
   * @param {!HTMLTableRowElement} baseRow The unexpanded row associated with
   *     the new row.
   * @return {!HTMLTableRowElement} The created tr element for the expanded row.
   */
  const createExpandedRow = function(state, baseRow) {
    assert(state);
    const guid = state.guid || '';
    const expandedRow = createTableRowElement();
    expandedRow.className = 'state-table-row';
    const emptyCell = createTableCellElement();
    emptyCell.style.border = 'none';
    expandedRow.appendChild(emptyCell);
    const detailCell = createTableCellElement();
    detailCell.id = guid ? idFromGuid(guid) : idFromType(state.type);
    detailCell.className = 'state-table-expanded-cell';
    detailCell.colSpan = baseRow.childNodes.length - 1;
    expandedRow.appendChild(detailCell);
    const selected = $('get-property-format').selectedIndex;
    const selectedId = $('get-property-format').options[selected].value;
    if (guid)
      handleNetworkDetail(guid, selectedId, detailCell);
    else
      handleDeviceDetail(state, selectedId, detailCell);
    return expandedRow;
  };

  /**
   * Requests network details and calls showDetail with the result.
   * @param {string} guid
   * @param {string} selectedId
   * @param {!HTMLTableCellElement} detailCell
   */
  const handleNetworkDetail = function(guid, selectedId, detailCell) {
    if (selectedId == 'shill') {
      chrome.send('getShillNetworkProperties', [guid]);
    } else if (selectedId == 'state') {
      networkConfigProxy.getNetworkState(guid)
          .then((responseParams) => {
            if (responseParams && responseParams.result) {
              showDetail(detailCell, responseParams.result);
            } else {
              showDetailError(
                  detailCell, 'GetNetworkState(' + guid + ') failed');
            }
          })
          .catch((error) => {
            showDetailError(detailCell, 'Mojo service failure: ' + error);
          });
    } else if (selectedId == 'managed') {
      chrome.networkingPrivate.getManagedProperties(guid, function(properties) {
        showDetail(detailCell, properties, chrome.runtime.lastError);
      });
    } else {
      chrome.networkingPrivate.getProperties(guid, function(properties) {
        showDetail(detailCell, properties, chrome.runtime.lastError);
      });
    }
  };

  /**
   * Requests network details and calls showDetail with the result.
   * @param {!networkUI.StateProperties} state
   * @param {string} selectedId
   * @param {!HTMLTableCellElement} detailCell
   */
  const handleDeviceDetail = function(state, selectedId, detailCell) {
    if (selectedId == 'shill') {
      chrome.send('getShillDeviceProperties', [state.type]);
    } else {
      showDetail(detailCell, state);
    }
  };

  /**
   * @param {!HTMLTableCellElement} detailCell
   * @param {!networkUI.NetworkStateProperties|!networkUI.DeviceStateProperties|
   *     !chrome.networkingPrivate.ManagedProperties|
   *     !chrome.networkingPrivate.NetworkProperties} state
   * @param {!Object=} error
   */
  const showDetail = function(detailCell, state, error) {
    if (error && error.message) {
      showDetailError(detailCell, error.message);
      return;
    }
    detailCell.textContent = JSON.stringify(state, null, '\t');
  };

  /**
   * @param {!HTMLTableCellElement} detailCell
   * @param {string} error
   */
  const showDetailError = function(detailCell, error) {
    detailCell.textContent = error;
  };

  /**
   * Callback invoked by Chrome after a getShillNetworkProperties call.
   *
   * @param {Array} args The requested Shill properties. Will contain
   *     just the 'GUID' and 'ShillError' properties if the call failed.
   */
  const getShillNetworkPropertiesResult = function(args) {
    const properties = args.shift();
    const guid = properties['GUID'];
    if (!guid) {
      console.error('No GUID in getShillNetworkPropertiesResult');
      return;
    }

    const detailCell = document.querySelector('td#' + idFromGuid(guid));
    if (!detailCell) {
      console.error('No cell for GUID: ' + guid);
      return;
    }

    if (properties['ShillError'])
      detailCell.textContent = properties['ShillError'];
    else
      detailCell.textContent = JSON.stringify(properties, null, '\t');
  };

  /**
   * Callback invoked by Chrome after a getShillDeviceProperties call.
   *
   * @param {Array} args The requested Shill properties. Will contain
   *     just the 'Type' and 'ShillError' properties if the call failed.
   */
  const getShillDevicePropertiesResult = function(args) {
    const properties = args.shift();
    const type = properties['Type'];
    if (!type) {
      console.error('No Type in getShillDevicePropertiesResult');
      return;
    }

    const detailCell = document.querySelector('td#' + idFromTypeString(type));
    if (!detailCell) {
      console.error('No cell for Type: ' + type);
      return;
    }

    if (properties['ShillError'])
      detailCell.textContent = properties['ShillError'];
    else
      detailCell.textContent = JSON.stringify(properties, null, '\t');
  };

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
   * Requests an update of all network info.
   */
  const requestNetworks = function() {
    networkConfigProxy
        .getNetworkStateList({
          filter: mojom.FilterType.kVisible,
          networkType: mojom.NetworkType.kAll,
          limit: mojom.kNoLimit,
        })
        .then((responseParams) => {
          onVisibleNetworksReceived(responseParams.result);
        });

    networkConfigProxy
        .getNetworkStateList({
          filter: mojom.FilterType.kConfigured,
          networkType: mojom.NetworkType.kAll,
          limit: mojom.kNoLimit,
        })
        .then((responseParams) => {
          onFavoriteNetworksReceived(responseParams.result);
        });

    networkConfigProxy.getDeviceStateList().then((responseParams) => {
      onDeviceStatesReceived(responseParams.result);
    });
  };

  /**
   * Requests the global policy dictionary and updates the page.
   */
  const requestGlobalPolicy = function() {
    chrome.networkingPrivate.getGlobalPolicy(function(policy) {
      document.querySelector('#global-policy').textContent =
          JSON.stringify(policy, null, '\t');
    });
  };

  /** Initialize NetworkUI state. */
  const init = function() {
    networkConfigProxy = network_config.MojoInterfaceProviderImpl.getInstance()
                             .getMojoServiceProxy();

    /** Set the refresh rate if the interval is found in the url. */
    const interval = parseQueryParams(window.location)['refresh'];
    if (interval && interval != '')
      setInterval(requestNetworks, parseInt(interval, 10) * 1000);
  };

  /**
   * Handles clicks on network items in the <cr-network-select> element by
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
    chrome.networkingPrivate.startConnect(networkState.guid, () => {
      const lastError = chrome.runtime.lastError;
      if (!lastError)
        return;
      const message = lastError.message;
      if (message == 'connecting' || message == 'connect-canceled' ||
          message == 'connected' || message == 'Error.InvalidNetworkGuid') {
        return;
      }
      console.error(
          'networkingPrivate.startConnect error: ' + message +
          ' For: ' + networkState.guid);
      chrome.send('showNetworkConfig', [networkState.guid]);
    });
  };

  /**
   * Gets network information from WebUI and sets custom items.
   */
  document.addEventListener('DOMContentLoaded', function() {
    const select = document.querySelector('cr-network-select');
    select.customItems = [
      {customItemName: 'Add WiFi', polymerIcon: 'cr:add', customData: 'WiFi'},
      {customItemName: 'Add VPN', polymerIcon: 'cr:add', customData: 'VPN'}
    ];
    select.addEventListener('network-item-selected', onNetworkItemSelected);
    $('cellular-activation-button').onclick = openCellularActivationUi;
    $('add-new-wifi-button').onclick = showAddNewWifi;
    $('refresh').onclick = requestNetworks;
    init();
    requestNetworks();
    requestGlobalPolicy();
  });

  document.addEventListener('custom-item-selected', function(event) {
    chrome.send('addNetwork', [event.detail.customData]);
  });

  return {
    getShillNetworkPropertiesResult: getShillNetworkPropertiesResult,
    getShillDevicePropertiesResult: getShillDevicePropertiesResult,
    openCellularActivationUiResult: openCellularActivationUiResult
  };
})();
