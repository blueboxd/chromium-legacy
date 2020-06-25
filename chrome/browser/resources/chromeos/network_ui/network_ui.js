// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element network debugging UI.
 */

Polymer({
  is: 'network-ui',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Names of the top level page tabs.
     * @type {!Array<String>}
     * @private
     */
    tabNames_: {
      type: Array,
      value: function() {
        return [
          this.i18n('generalTab'),
          this.i18n('networkHealthTab'),
          this.i18n('networkLogsTab'),
          this.i18n('networkStateTab'),
          this.i18n('networkSelectTab'),
        ];
      },
    },

    /**
     * Index of the selected top level page tab.
     * @private
     */
    selectedTab_: {
      type: Number,
      value: 0,
    },
  },

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @type {!network_ui.NetworkUIBrowserProxy} */
  browserProxy_: network_ui.NetworkUIBrowserProxyImpl.getInstance(),

  /** @override */
  attached() {
    this.networkConfig_ =
        chromeos.networkConfig.mojom.CrosNetworkConfig.getRemote();

    const select = this.$$('network-select');
    select.customItems = [
      {
        customItemName: 'addWiFiListItemName',
        polymerIcon: 'cr:add',
        customData: 'WiFi'
      },
    ];

    this.$$('#import-onc').value = '';

    this.requestGlobalPolicy_();
  },

  /** @private */
  openCellularActivationUi_() {
    this.browserProxy_.openCellularActivationUi().then((response) => {
      const didOpenActivationUi = response.shift();
      this.$$('#cellular-error-text').hidden = didOpenActivationUi;
    });
  },

  /** @private */
  showAddNewWifi_() {
    this.browserProxy_.showAddNewWifi();
  },

  /**
   * Handles the ONC file input change event.
   * @param {!Event} event
   * @private
   */
  onImportOncChange_(event) {
    const file = event.target.files[0];
    event.stopPropagation();
    if (!file)
      return;
    const reader = new FileReader();
    reader.onloadend = (e) => {
      const content = reader.result;
      if (!content || typeof (content) != 'string') {
        console.error('File not read' + file);
        return;
      }
      this.browserProxy_.importONC(content).then((response) => {
        this.importONCResponse_(response);
      });
    };
    reader.readAsText(file);
  },

  /**
   * Handles the chrome 'importONC' response.
   * @param {Array} args
   * @private
   */
  importONCResponse_(args) {
    const result = args.shift();
    const isError = args.shift();
    const resultDiv = this.$$('#onc-import-result');
    resultDiv.innerText = result;
    resultDiv.classList.toggle('error', isError);
    this.$$('#import-onc').value = '';
  },

  /**
   * Requests the global policy dictionary and updates the page.
   * @private
   */
  requestGlobalPolicy_() {
    this.networkConfig_.getGlobalPolicy().then(result => {
      this.$$('#global-policy').textContent =
          JSON.stringify(result.result, null, '\t');
    });
  },

  /**
   * Handles clicks on network items in the <network-select> element by
   * attempting a connection to the selected network or requesting a password
   * if the network requires a password.
   * @param {!Event<!OncMojo.NetworkStateProperties>} event
   * @private
   */
  onNetworkItemSelected_(event) {
    const networkState = event.detail;

    // If the network is already connected, show network details.
    if (OncMojo.connectionStateIsConnected(networkState.connectionState)) {
      this.browserProxy_.showNetworkDetails(networkState.guid);
      return;
    }

    // If the network is not connectable, show a configuration dialog.
    if (networkState.connectable === false || networkState.errorState) {
      this.browserProxy_.showNetworkConfig(networkState.guid);
      return;
    }

    // Otherwise, connect.
    this.networkConfig_.startConnect(networkState.guid).then(response => {
      if (response.result ==
          chromeos.networkConfig.mojom.StartConnectResult.kSuccess) {
        return;
      }
      console.error(
          'startConnect error for: ' + networkState.guid + ' Result: ' +
          response.result.toString() + ' Message: ' + response.message);
      this.browserProxy_.showNetworkConfig(networkState.guid);
    });
  },

  /**
   * @param {!Event<!{detail:{customData: string}}>} event
   * @private
   */
  onCustomItemSelected_(event) {
    this.browserProxy_.addNetwork(event.detail.customData);
  },
});
