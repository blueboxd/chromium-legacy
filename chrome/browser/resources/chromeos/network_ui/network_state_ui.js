// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {!OncMojo.DeviceStateProperties|!OncMojo.NetworkStateProperties}
 */
OncMojo.StateProperties;

Polymer({
  is: 'network-state-ui',

  properties: {},

  /**
   * Properties to display in the network state table. Each entry can be either
   * a single state field or an array of state fields. If more than one is
   * specified then the first non empty value is used.
   * @const
   */
  NETWORK_STATE_FIELDS: [
    'guid', 'name', 'type', 'connectionState', 'connectable', 'errorState',
    'wifi.security', ['cellular.networkTechnology', 'EAP.EAP'],
    'cellular.activationState', 'cellular.roaming', 'wifi.frequency',
    'wifi.signalStrength'
  ],

  /** @const */
  FAVORITE_STATE_FIELDS: ['guid', 'name', 'type', 'source'],

  /** @const */
  DEVICE_STATE_FIELDS: ['type', 'deviceState'],

  /**
   * This UI will use both the networkingPrivate extension API and the
   * networkConfig mojo API until we provide all of the required functionality
   * in networkConfig. TODO(stevenjb): Remove use of networkingPrivate api.
   * @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote}
   */
  networkConfig: null,

  /** @override */
  created() {
    this.networkConfig = network_config.MojoInterfaceProviderImpl.getInstance()
                             .getMojoServiceRemote();
  },

  /** @override */
  attached() {
    /** Set the refresh rate if the interval is found in the url. */
    const interval = new URL(window.location.href).searchParams.get('refresh');
    if (interval) {
      window.setInterval(() => {
        this.requestNetworks();
      }, parseInt(interval, 10) * 1000);
    }
    this.requestNetworks();
  },

  /**
   * Creates and returns a typed HTMLTableCellElement.
   *
   * @return {!HTMLTableCellElement} A new td element.
   */
  createTableCellElement() {
    return /** @type {!HTMLTableCellElement} */ (document.createElement('td'));
  },

  /**
   * Creates and returns a typed HTMLTableRowElement.
   *
   * @return {!HTMLTableRowElement} A new tr element.
   */
  createTableRowElement() {
    return /** @type {!HTMLTableRowElement} */ (document.createElement('tr'));
  },

  /**
   * @param {string} key
   * @param {number|string|undefined} value
   * @return {string}
   */
  getOncTypeString(key, value) {
    if (value === undefined) {
      return '';
    }
    if (key == 'type' && value == 'etherneteap') {
      // Special case, not in production UI.
      return 'EthernetEAP';
    }
    return /** @type {string}*/ (OncMojo.getTypeString(key, value));
  },

  /**
   * Returns the ONC data property for |state| associated with a key. Used
   * to access properties in the state by |key| which may may refer to a
   * nested property, e.g. 'WiFi.Security'. If any part of a nested key is
   * missing, this will return undefined.
   *
   * @param {!OncMojo.StateProperties} state
   * @param {string} key The ONC key for the property.
   * @return {*} The value associated with the property or undefined if the
   *     key (any part of it) is not defined.
   */
  getOncProperty(state, key) {
    let dict = /** @type {!Object} */ (state);
    const keys = key.split('.');
    while (keys.length > 1) {
      const k = keys.shift();
      dict = dict[k];
      if (!dict || typeof dict != 'object')
        return undefined;
    }
    const k = keys.shift();
    return this.getOncTypeString(k, dict[k]);
  },

  /**
   * Creates a cell with a button for expanding a network state table row.
   *
   * @param {!OncMojo.StateProperties} state
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     given value.
   */
  createStateTableExpandButton(state) {
    const cell = this.createTableCellElement();
    cell.className = 'state-table-expand-button-cell';
    const button = document.createElement('button');
    button.addEventListener('click', (event) => {
      this.toggleExpandRow(/** @type {!HTMLElement} */ (event.target), state);
    });
    button.className = 'state-table-expand-button';
    button.textContent = '+';
    cell.appendChild(button);
    return cell;
  },

  /**
   * Creates a cell with an icon representing the network state.
   *
   * @param {!OncMojo.StateProperties} state
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     icon.
   */
  createStateTableIcon(state) {
    const cell = this.createTableCellElement();
    cell.className = 'state-table-icon-cell';
    const icon = /** @type {!NetworkIconElement} */ (
        document.createElement('network-icon'));
    icon.isListItem = true;
    icon.networkState = OncMojo.getDefaultNetworkState(state.type);
    cell.appendChild(icon);
    return cell;
  },

  /**
   * Creates a cell in the network state table.
   *
   * @param {*} value Content in the cell.
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     given value.
   */
  createStateTableCell(value) {
    const cell = this.createTableCellElement();
    cell.textContent = value || '';
    return cell;
  },

  /**
   * Creates a row in the network state table.
   *
   * @param {Array} stateFields The state fields to use for the row.
   * @param {!OncMojo.StateProperties} state
   * @return {!HTMLTableRowElement} The created tr element that contains the
   *     network state information.
   */
  createStateTableRow(stateFields, state) {
    const row = this.createTableRowElement();
    row.className = 'state-table-row';
    row.appendChild(this.createStateTableExpandButton(state));
    row.appendChild(this.createStateTableIcon(state));
    for (let i = 0; i < stateFields.length; ++i) {
      const field = stateFields[i];
      let value;
      if (typeof field == 'string') {
        value = this.getOncProperty(state, field);
      } else {
        for (let j = 0; j < field.length; ++j) {
          value = this.getOncProperty(state, field[j]);
          if (value != undefined)
            break;
        }
      }
      if (field == 'guid')
        value = value.slice(0, 8);
      row.appendChild(this.createStateTableCell(value));
    }
    return row;
  },

  /**
   * Creates a table for networks or favorites.
   *
   * @param {string} tablename The name of the table to be created.
   * @param {!Array<string>} stateFields The list of fields for the table.
   * @param {!Array<!OncMojo.StateProperties>} states
   */
  createStateTable(tablename, stateFields, states) {
    const table = this.$$('#' + tablename);
    const oldRows = table.querySelectorAll('.state-table-row');
    for (let i = 0; i < oldRows.length; ++i)
      table.removeChild(oldRows[i]);
    states.forEach((state) => {
      table.appendChild(this.createStateTableRow(stateFields, state));
    });
  },

  /**
   * Returns a valid HTMLElement id from |guid|.
   *
   * @param {string} guid A GUID which may start with a digit.
   * @return {string} A valid HTMLElement id.
   */
  idFromGuid(guid) {
    return '_' + guid.replace(/[{}]/g, '');
  },

  /**
   * Returns a valid HTMLElement id from |type|. Note: |type| may be a Shill
   * type or an ONC type, so strip _ and convert to lowercase to unify them.
   *
   * @param {string} type A Shill or ONC network type
   * @return {string} A valid HTMLElement id.
   */
  idFromTypeString(type) {
    return '_' + type.replace(/[{}_]/g, '').toLowerCase();
  },

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @return {string} A valid HTMLElement id.
   */
  idFromType(type) {
    return this.idFromTypeString(this.getOncTypeString('type', type));
  },

  /**
   * This callback function is triggered when visible networks are received.
   *
   * @param {!Array<!OncMojo.NetworkStateProperties>} states
   */
  onVisibleNetworksReceived(states) {
    this.createStateTable(
        'network-state-table', this.NETWORK_STATE_FIELDS, states);
  },

  /**
   * This callback function is triggered when favorite networks are received.
   *
   * @param {!Array<!OncMojo.NetworkStateProperties>} states
   */
  onFavoriteNetworksReceived(states) {
    this.createStateTable(
        'favorite-state-table', this.FAVORITE_STATE_FIELDS, states);
  },

  /**
   * This callback function is triggered when device states are received.
   *
   * @param {!Array<!OncMojo.DeviceStateProperties>} states
   */
  onDeviceStatesReceived(states) {
    this.createStateTable(
        'device-state-table', this.DEVICE_STATE_FIELDS, states);
  },

  /** @return {string} */
  getSelectedFormat() {
    const formatSelect = this.$$('#get-property-format');
    return formatSelect.options[formatSelect.selectedIndex].value;
  },

  /**
   * Toggles the button state and add or remove a row displaying the complete
   * state information for a row.
   *
   * @param {!HTMLElement} btn The button that was clicked.
   * @param {!OncMojo.StateProperties} state
   */
  toggleExpandRow(btn, state) {
    const cell = btn.parentNode;
    const row = /** @type {!HTMLTableRowElement} */ (cell.parentNode);
    if (btn.textContent == '-') {
      btn.textContent = '+';
      row.parentNode.removeChild(row.nextSibling);
    } else {
      btn.textContent = '-';
      const expandedRow = this.createExpandedRow(state, row);
      row.parentNode.insertBefore(expandedRow, row.nextSibling);
    }
  },

  /**
   * Creates the expanded row for displaying the complete state as JSON.
   *
   * @param {!OncMojo.StateProperties} state
   * @param {!HTMLTableRowElement} baseRow The unexpanded row associated with
   *     the new row.
   * @return {!HTMLTableRowElement} The created tr element for the expanded row.
   */
  createExpandedRow(state, baseRow) {
    assert(state);
    const guid = state.guid || '';
    const expandedRow = this.createTableRowElement();
    expandedRow.className = 'state-table-row';
    const emptyCell = this.createTableCellElement();
    emptyCell.style.border = 'none';
    expandedRow.appendChild(emptyCell);
    const detailCell = this.createTableCellElement();
    detailCell.id = guid ? this.idFromGuid(guid) : this.idFromType(state.type);
    detailCell.className = 'state-table-expanded-cell';
    detailCell.colSpan = baseRow.childNodes.length - 1;
    expandedRow.appendChild(detailCell);
    if (guid)
      this.handleNetworkDetail(guid, this.getSelectedFormat(), detailCell);
    else
      this.handleDeviceDetail(state, this.getSelectedFormat(), detailCell);
    return expandedRow;
  },

  /**
   * Requests network details and calls showDetail with the result.
   * @param {string} guid
   * @param {string} selectedId
   * @param {!HTMLTableCellElement} detailCell
   */
  handleNetworkDetail(guid, selectedId, detailCell) {
    if (selectedId == 'shill') {
      chrome.send('getShillNetworkProperties', [guid]);
    } else if (selectedId == 'state') {
      this.networkConfig.getNetworkState(guid)
          .then((responseParams) => {
            if (responseParams && responseParams.result) {
              this.showDetail(detailCell, responseParams.result);
            } else {
              this.showDetailError(
                  detailCell, 'getNetworkState(' + guid + ') failed');
            }
          })
          .catch((error) => {
            this.showDetailError(detailCell, 'Mojo service failure: ' + error);
          });
    } else if (selectedId == 'managed') {
      this.networkConfig.getManagedProperties(guid)
          .then((responseParams) => {
            if (responseParams && responseParams.result) {
              this.showDetail(detailCell, responseParams.result);
            } else {
              this.showDetailError(
                  detailCell, 'getManagedProperties(' + guid + ') failed');
            }
          })
          .catch((error) => {
            this.showDetailError(detailCell, 'Mojo service failure: ' + error);
          });
    } else {
      chrome.networkingPrivate.getProperties(guid, (properties) => {
        this.showDetail(detailCell, properties, chrome.runtime.lastError);
      });
    }
  },

  /**
   * Requests network details and calls showDetail with the result.
   * @param {!OncMojo.StateProperties} state
   * @param {string} selectedId
   * @param {!HTMLTableCellElement} detailCell
   */
  handleDeviceDetail(state, selectedId, detailCell) {
    if (selectedId == 'shill') {
      chrome.send(
          'getShillDeviceProperties',
          [OncMojo.getNetworkTypeString(state.type)]);
    } else {
      this.showDetail(detailCell, state);
    }
  },

  /**
   * @param {!HTMLTableCellElement} detailCell
   * @param {!OncMojo.NetworkStateProperties|!OncMojo.DeviceStateProperties|
   *         !chromeos.networkConfig.mojom.ManagedProperties|
   *         !chrome.networkingPrivate.NetworkProperties} state
   * @param {!Object=} error
   */
  showDetail(detailCell, state, error) {
    if (error && error.message) {
      this.showDetailError(detailCell, error.message);
      return;
    }
    detailCell.textContent = JSON.stringify(state, null, '\t');
  },

  /**
   * @param {!HTMLTableCellElement} detailCell
   * @param {string} error
   */
  showDetailError(detailCell, error) {
    detailCell.textContent = error;
  },

  /**
   * Callback invoked by Chrome after a getShillNetworkProperties call.
   *
   * @param {Array} args The requested Shill properties. Will contain
   *     just the 'GUID' and 'ShillError' properties if the call failed.
   */
  getShillNetworkPropertiesResult(args) {
    const properties = args.shift();
    const guid = properties['GUID'];
    if (!guid) {
      console.error('No GUID in getShillNetworkPropertiesResult');
      return;
    }

    const detailCell = this.$$('td#' + this.idFromGuid(guid));
    if (!detailCell) {
      console.error('No cell for GUID: ' + guid);
      return;
    }

    if (properties['ShillError'])
      detailCell.textContent = properties['ShillError'];
    else
      detailCell.textContent = JSON.stringify(properties, null, '\t');
  },

  /**
   * Callback invoked by Chrome after a getShillDeviceProperties call.
   *
   * @param {Array} args The requested Shill properties. Will contain
   *     just the 'Type' and 'ShillError' properties if the call failed.
   */
  getShillDevicePropertiesResult(args) {
    const properties = args.shift();
    const type = properties['Type'];
    if (!type) {
      console.error('No Type in getShillDevicePropertiesResult');
      return;
    }

    const detailCell = this.$$('td#' + this.idFromTypeString(type));
    if (!detailCell) {
      console.error('No cell for Type: ' + type);
      return;
    }

    if (properties['ShillError'])
      detailCell.textContent = properties['ShillError'];
    else
      detailCell.textContent = JSON.stringify(properties, null, '\t');
  },

  /**
   * Callback invoked by Chrome after a getShillEthernetEAP call.
   * @param {?Object} state The requested Shill properties. Will be null if no
   *     EAP properties exist.
   */
  getShillEthernetEAPResult(state) {
    const states = [];
    if (state) {
      states.push(state);
    }
    this.createStateTable(
        'ethernet-eap-state-table', this.FAVORITE_STATE_FIELDS, states);
  },

  /**
   * Requests an update of all network info.
   */
  requestNetworks() {
    const mojom = chromeos.networkConfig.mojom;
    this.networkConfig
        .getNetworkStateList({
          filter: mojom.FilterType.kVisible,
          networkType: mojom.NetworkType.kAll,
          limit: mojom.NO_LIMIT,
        })
        .then((responseParams) => {
          this.onVisibleNetworksReceived(responseParams.result);
        });

    this.networkConfig
        .getNetworkStateList({
          filter: mojom.FilterType.kConfigured,
          networkType: mojom.NetworkType.kAll,
          limit: mojom.NO_LIMIT,
        })
        .then((responseParams) => {
          this.onFavoriteNetworksReceived(responseParams.result);
        });

    this.networkConfig.getDeviceStateList().then((responseParams) => {
      this.onDeviceStatesReceived(responseParams.result);
    });

    // Only request EthernetEAP properties when the 'shill' format is selected.
    if (this.getSelectedFormat() == 'shill') {
      chrome.send('getShillEthernetEAP');
    }
  },
});
