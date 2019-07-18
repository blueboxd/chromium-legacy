// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('InternetDetailPage', function() {
  /** @type {InternetDetailPageElement} */
  let internetDetailPage = null;

  /** @type {NetworkingPrivate} */
  let api_;

  /** @type {Object} */
  let prefs_ = {
    'vpn_config_allowed': {
      key: 'vpn_config_allowed',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    // Added use_shared_proxies because triggering a change in prefs_ without
    // it will fail a "Pref is missing" assertion in the network-proxy-section
    'settings': {
      'use_shared_proxies': {
        key: 'use_shared_proxies',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
  };

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
      OncTypeWiMAX: 'OncTypeWiMAX',
      networkListItemConnected: 'networkListItemConnected',
      networkListItemConnecting: 'networkListItemConnecting',
      networkListItemConnectingTo: 'networkListItemConnectingTo',
      networkListItemNotConnected: 'networkListItemNotConnected',
      networkListItemNoNetwork: 'networkListItemNoNetwork',
      vpnNameTemplate: 'vpnNameTemplate',
    };

    api_ = new chrome.FakeNetworkingPrivate();

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      internetDetailPage.async(resolve);
    });
  }

  function setNetworksForTest(networks) {
    api_.resetForTest();
    api_.addNetworksForTest(networks);
  }

  function getAllowSharedProxy() {
    const proxySection = internetDetailPage.$$('network-proxy-section');
    assertTrue(!!proxySection);
    const allowShared = proxySection.$$('#allowShared');
    assertTrue(!!allowShared);
    return allowShared;
  }

  function getButton(buttonId) {
    const button = internetDetailPage.$$(`#${buttonId}`);
    assertTrue(!!button);
    return button;
  }

  setup(function() {
    PolymerTest.clearBody();
    internetDetailPage =
        document.createElement('settings-internet-detail-page');
    assertTrue(!!internetDetailPage);
    api_.resetForTest();
    internetDetailPage.networkingPrivate = api_;
    internetDetailPage.prefs = Object.assign({}, prefs_);
    document.body.appendChild(internetDetailPage);
    return flushAsync();
  });

  teardown(function() {
    internetDetailPage.remove();
    internetDetailPage = null;
    settings.resetRouteForTesting();
  });

  suite('DetailsPage', function() {
    test('LoadPage', function() {});

    test('WiFi', function() {
      api_.enableNetworkType('WiFi');
      setNetworksForTest([{GUID: 'wifi1_guid', Name: 'wifi1', Type: 'WiFi'}]);
      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      assertEquals('wifi1_guid', internetDetailPage.guid);
      return flushAsync().then(() => {
        return Promise.all([
          api_.whenCalled('getManagedProperties'),
        ]);
      });
    });

    test('Proxy Unshared', function() {
      api_.enableNetworkType('WiFi');
      setNetworksForTest([{
        GUID: 'wifi_user_guid',
        Name: 'wifi_user',
        Type: 'WiFi',
        Source: 'User'
      }]);
      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const proxySection = internetDetailPage.$$('network-proxy-section');
        assertTrue(!!proxySection);
        const allowShared = proxySection.$$('#allowShared');
        assertTrue(!!allowShared);
        assertTrue(allowShared.hasAttribute('hidden'));
      });
    });

    test('Proxy Shared', function() {
      api_.enableNetworkType('WiFi');
      setNetworksForTest([{
        GUID: 'wifi_user_guid',
        Name: 'wifi_user',
        Type: 'WiFi',
        Source: 'Device'
      }]);
      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        let allowShared = getAllowSharedProxy();
        assertFalse(allowShared.hasAttribute('hidden'));
        assertFalse(allowShared.disabled);
      });
    });

    // When proxy settings are managed by a user policy they may respect the
    // allowd_shared_proxies pref so #allowShared should be visible.
    // TOD(stevenjb): Improve this: crbug.com/662529.
    test('Proxy Shared User Managed', function() {
      api_.enableNetworkType('WiFi');
      setNetworksForTest([{
        GUID: 'wifi_user_guid',
        Name: 'wifi_user',
        Type: 'WiFi',
        Source: 'Device',
        ProxySettings: {
          Type: {Active: 'Manual', Effective: 'UserPolicy', UserEditable: false}
        }
      }]);
      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        let allowShared = getAllowSharedProxy();
        assertFalse(allowShared.hasAttribute('hidden'));
        assertFalse(allowShared.disabled);
      });
    });

    // When proxy settings are managed by a device policy they may respect the
    // allowd_shared_proxies pref so #allowShared should be visible.
    test('Proxy Shared Device Managed', function() {
      api_.enableNetworkType('WiFi');
      setNetworksForTest([{
        GUID: 'wifi_user_guid',
        Name: 'wifi_user',
        Type: 'WiFi',
        Source: 'Device',
        ProxySettings: {
          Type: {
            Active: 'Manual',
            Effective: 'DevicePolicy',
            DeviceEditable: false
          }
        }
      }]);
      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        let allowShared = getAllowSharedProxy();
        assertFalse(allowShared.hasAttribute('hidden'));
        assertFalse(allowShared.disabled);
      });
    });

    test('VPN config allowed', function() {
      api_.enableNetworkType('VPN');
      setNetworksForTest([{
        GUID: 'vpn_guid',
        Name: 'vpn_user',
        Type: 'VPN',
      }]);
      internetDetailPage.init('vpn_guid', 'VPN', 'vpn_user');
      prefs_.vpn_config_allowed.value = true;
      internetDetailPage.prefs = Object.assign({}, prefs_);
      return flushAsync().then(() => {
        const disconnectButton = getButton('disconnect');
        assertFalse(disconnectButton.hasAttribute('enforced_'));
        assertFalse(!!disconnectButton.$$('cr-policy-pref-indicator'));
      });
    });

    test('VPN config disallowed', function() {
      api_.enableNetworkType('VPN');
      setNetworksForTest([{
        GUID: 'vpn_guid',
        Name: 'vpn_user',
        Type: 'VPN',
      }]);
      internetDetailPage.init('vpn_guid', 'VPN', 'vpn_user');
      prefs_.vpn_config_allowed.value = false;
      internetDetailPage.prefs = Object.assign({}, prefs_);
      return flushAsync().then(() => {
        const disconnectButton = getButton('disconnect');
        assertTrue(disconnectButton.hasAttribute('enforced_'));
        assertTrue(!!disconnectButton.$$('cr-policy-pref-indicator'));
      });
    });

    test('Connect button disabled on cellular scan', function() {
      api_.enableNetworkType('Cellular');
      setNetworksForTest([{
        GUID: 'cell_guid',
        Name: 'cell_user',
        Type: 'Cellular',
        ConnectionState: 'NotConnected',
        Cellular: {SIMLockStatus: {LockType: 'sim-pin'}}
      }]);
      internetDetailPage.init('cell_guid', 'Cellular', 'cell_user');
      return flushAsync().then(() => {
        const connectButton = getButton('connect');
        assertTrue(connectButton.hasAttribute('disabled'));
      });
    });

    test.only(
        'Auto Connect toggle updates properly after GUID change', function() {
          api_.enableNetworkType('WiFi');
          setNetworksForTest([
            {
              GUID: 'wifi1_guid',
              Name: 'wifi1',
              Type: 'WiFi',
              Source: CrOnc.Source.USER,
              WiFi: {AutoConnect: true}
            },
            {
              GUID: 'wifi2_guid',
              Name: 'wifi1',
              Type: 'WiFi',
              Source: CrOnc.Source.USER,
              WiFi: {AutoConnect: false}
            }
          ]);
          internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi_user');
          return flushAsync()
              .then(() => {
                assertTrue(internetDetailPage.$$('#autoConnectToggle').checked);
                internetDetailPage.init('wifi2_guid', 'WiFi', 'wifi_user');
                return flushAsync();
              })
              .then(() => {
                assertFalse(
                    internetDetailPage.$$('#autoConnectToggle').checked);
              });
        });
  });
});
