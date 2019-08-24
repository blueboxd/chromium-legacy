// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('InternetSubpage', function() {
  /** @type {?SettingsInternetSubpageElement} */
  let internetSubpage = null;

  /** @type {?NetworkingPrivate} */
  let api_ = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      internetAddConnection: 'internetAddConnection',
      internetAddConnectionExpandA11yLabel:
          'internetAddConnectionExpandA11yLabel',
      internetAddConnectionNotAllowed: 'internetAddConnectionNotAllowed',
      internetAddThirdPartyVPN: 'internetAddThirdPartyVPN',
      internetAddVPN: 'internetAddVPN',
      internetAddArcVPN: 'internetAddArcVPN',
      internetAddArcVPNProvider: 'internetAddArcVPNProvider',
      internetAddWiFi: 'internetAddWiFi',
      internetDetailPageTitle: 'internetDetailPageTitle',
      internetKnownNetworksPageTitle: 'internetKnownNetworksPageTitle',
    });

    CrOncStrings = {
      OncTypeCellular: 'OncTypeCellular',
      OncTypeEthernet: 'OncTypeEthernet',
      OncTypeMobile: 'OncTypeMobile',
      OncTypeTether: 'OncTypeTether',
      OncTypeVPN: 'OncTypeVPN',
      OncTypeWiFi: 'OncTypeWiFi',
      networkListItemConnected: 'networkListItemConnected',
      networkListItemConnecting: 'networkListItemConnecting',
      networkListItemConnectingTo: 'networkListItemConnectingTo',
      networkListItemNotConnected: 'networkListItemNotConnected',
      networkListItemNoNetwork: 'networkListItemNoNetwork',
      vpnNameTemplate: 'vpnNameTemplate',
    };

    api_ = new chrome.FakeNetworkingPrivate();
    mojoApi_ = new FakeNetworkConfig(api_);
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function setNetworksForTest(type, networks) {
    api_.resetForTest();
    api_.enableNetworkType(type);
    api_.addNetworksForTest(networks);
    internetSubpage.defaultNetwork = networks[0];
    internetSubpage.deviceState = mojoApi_.getDeviceStateForTest(type);
    api_.onNetworkListChanged.callListeners();
  }

  function setArcVpnProvidersForTest(arcVpnProviders) {
    cr.webUIListenerCallback('sendArcVpnProviders', arcVpnProviders);
  }

  setup(function() {
    PolymerTest.clearBody();
    internetSubpage = document.createElement('settings-internet-subpage');
    assertTrue(!!internetSubpage);
    api_.resetForTest();
    internetSubpage.networkingPrivate = api_;
    document.body.appendChild(internetSubpage);
    internetSubpage.init();
    return flushAsync();
  });

  teardown(function() {
    internetSubpage.remove();
    internetSubpage = null;
    settings.resetRouteForTesting();
  });

  suite('SubPage', function() {
    test('WiFi', function() {
      setNetworksForTest('WiFi', [
        {GUID: 'wifi1_guid', Name: 'wifi1', Type: 'WiFi'},
        {GUID: 'wifi12_guid', Name: 'wifi2', Type: 'WiFi'},
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const toggle = internetSubpage.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
      });
    });

    test('Tether', function() {
      setNetworksForTest('Tether', [
        {GUID: 'tether1_guid', Name: 'tether1', Type: 'Tether'},
        {GUID: 'tether2_guid', Name: 'tether2', Type: 'Tether'},
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const toggle = internetSubpage.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
        const tetherToggle = internetSubpage.$$('#tetherEnabledButton');
        // No separate tether toggle when Celular is not available; the
        // primary toggle enables or disables Tether in that case.
        assertFalse(!!tetherToggle);
      });
    });

    test('Tether plus Cellular', function() {
      api_.enableNetworkType('Tether');
      setNetworksForTest('Cellular', [
        {GUID: 'cellular1_guid', Name: 'cellular1', Type: 'Cellular'},
        {GUID: 'tether1_guid', Name: 'tether1', Type: 'Tether'},
        {GUID: 'tether2_guid', Name: 'tether2', Type: 'Tether'},
      ]);
      internetSubpage.tetherDeviceState =
          mojoApi_.deviceToMojo({Type: 'Tether', State: 'Enabled'});
      return flushAsync().then(() => {
        assertEquals(3, internetSubpage.networkStateList_.length);
        const toggle = internetSubpage.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(3, networkList.networks.length);
        const tetherToggle = internetSubpage.$$('#tetherEnabledButton');
        assertTrue(!!tetherToggle);
        assertFalse(tetherToggle.disabled);
      });
    });

    test('VPN', function() {
      setNetworksForTest('VPN', [
        {GUID: 'vpn1_guid', Name: 'vpn1', Type: 'VPN'},
        {GUID: 'vpn2_guid', Name: 'vpn1', Type: 'VPN'},
        {
          GUID: 'third_party1_vpn1_guid',
          Name: 'vpn3',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id1', ProviderName: 'pname1'}
          }
        },
        {
          GUID: 'third_party1_vpn2_guid',
          Name: 'vpn4',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id1', ProviderName: 'pname1'}
          }
        },
        {
          GUID: 'third_party2_vpn1_guid',
          Name: 'vpn5',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id2', ProviderName: 'pname2'}
          }
        },
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
        // TODO(stevenjb): Implement fake management API and test third
        // party provider sections.
      });
    });
  });
});
