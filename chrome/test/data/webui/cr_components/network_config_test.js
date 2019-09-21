// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('network-config', function() {
  var networkConfig;

  /** @type {NetworkingPrivate} */
  var api_;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  suiteSetup(function() {
    api_ = new chrome.FakeNetworkingPrivate();  // For certificates
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    CrOncTest.overrideCrOncStrings();
  });

  function setNetworkConfig(properties) {
    assert(properties.guid);
    mojoApi_.setManagedPropertiesForTest(properties);
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.networkingPrivate = api_;
    networkConfig.guid = properties.guid;
    networkConfig.managedProperties = properties;
  }

  function setNetworkType(type, security) {
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.networkingPrivate = api_;
    networkConfig.type = OncMojo.getNetworkTypeString(type);
    if (security !== undefined) {
      networkConfig.securityType = security;
    }
  }

  function initNetworkConfig() {
    document.body.appendChild(networkConfig);
    networkConfig.init();
    Polymer.dom.flush();
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      networkConfig.async(resolve);
    });
  }

  suite('New WiFi Config', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kWiFi);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      assertTrue(!!networkConfig.$$('#share'));
      assertTrue(!!networkConfig.$$('#ssid'));
      assertTrue(!!networkConfig.$$('#security'));
      assertFalse(networkConfig.$$('#security').disabled);
    });
  });

  suite('Existing WiFi Config', function() {
    setup(function() {
      mojoApi_.resetForTest();
      const wifi1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kWiFi, 'someguid', '');
      wifi1.name = OncMojo.createManagedString('somename');
      wifi1.source = chromeos.networkConfig.mojom.OncSource.kDevice;
      wifi1.wifi.security = chromeos.networkConfig.mojom.SecurityType.kWepPsk;
      setNetworkConfig(wifi1);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      return flushAsync().then(() => {
        assertEquals('someguid', networkConfig.managedProperties.guid);
        assertEquals(
            'somename', networkConfig.managedProperties.name.activeValue);
        assertFalse(!!networkConfig.$$('#share'));
        assertTrue(!!networkConfig.$$('#ssid'));
        assertTrue(!!networkConfig.$$('#security'));
        assertTrue(networkConfig.$$('#security').disabled);
      });
    });
  });

  suite('Share', function() {
    setup(function() {
      mojoApi_.resetForTest();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    function setLoginOrGuest() {
      // Networks must be shared.
      networkConfig.shareAllowEnable = false;
      networkConfig.shareDefault = true;
    }

    function setKiosk() {
      // New networks can not be shared.
      networkConfig.shareAllowEnable = false;
      networkConfig.shareDefault = false;
    }

    function setAuthenticated() {
      // Logged in users can share new networks.
      networkConfig.shareAllowEnable = true;
      // Authenticated networks default to not shared.
      networkConfig.shareDefault = false;
    }

    function setCertificatesForTest() {
      const kHash1 = 'TESTHASH1', kHash2 = 'TESTHASH2';
      var clientCert = {hash: kHash1, hardwareBacked: true, deviceWide: false};
      var caCert = {hash: kHash2, hardwareBacked: true, deviceWide: true};
      api_.setCertificatesForTest(
          {serverCaCertificates: [caCert], userCertificates: [clientCert]});
      this.selectedUserCertHash_ = kHash1;
      this.selectedServerCaHash_ = kHash2;
    }

    test('New Config: Login or guest', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setLoginOrGuest();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Kiosk', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setKiosk();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Not secure', function() {
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Authenticated, Secure', function() {
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertFalse(share.checked);
      });
    });

    // Existing networks hide the shared control in the config UI.
    test('Existing Hides Shared', function() {
      const wifi1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kWiFi, 'someguid', '');
      wifi1.source = chromeos.networkConfig.mojom.OncSource.kUser;
      wifi1.wifi.security = chromeos.networkConfig.mojom.SecurityType.kWepPsk;
      setNetworkConfig(wifi1);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        assertFalse(!!networkConfig.$$('#share'));
      });
    });

    test('Ethernet', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kEthernet, 'ethernetguid',
          '');
      eth.ethernet.authentication = OncMojo.createManagedString('None');
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('ethernetguid', networkConfig.guid);
        assertEquals(
            chromeos.networkConfig.mojom.SecurityType.kNone,
            networkConfig.securityType);
        let outer = networkConfig.$$('#outer');
        assertFalse(!!outer);
      });
    });

    test('Ethernet EAP', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kEthernet, 'eapguid', '');
      eth.ethernet.authentication = OncMojo.createManagedString('8021x');
      eth.ethernet.eap = {outer: OncMojo.createManagedString('PEAP')};
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('eapguid', networkConfig.guid);
        assertEquals(
            chromeos.networkConfig.mojom.SecurityType.kWpaEap,
            networkConfig.securityType);
        assertEquals(
            'PEAP',
            networkConfig.managedProperties.ethernet.eap.outer.activeValue);
        let outer = networkConfig.$$('#outer');
        assertTrue(!!outer);
        assertTrue(!outer.disabled);
        assertEquals('PEAP', outer.value);
      });
    });

    test('WiFi EAP TLS', function() {
      const wifi1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kWiFi, 'eaptlsguid', '');
      wifi1.wifi.security = chromeos.networkConfig.mojom.SecurityType.kWpaEap;
      wifi1.wifi.eap = {outer: OncMojo.createManagedString('EAP-TLS')};
      setNetworkConfig(wifi1);
      setCertificatesForTest();
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let outer = networkConfig.$$('#outer');
        assertEquals('EAP-TLS', outer.value);

        // check that a valid client user certificate is selected
        let clientCert = networkConfig.$$('#userCert').$$('select').value;
        assertTrue(!!clientCert);
        let caCert = networkConfig.$$('#serverCa').$$('select').value;
        assertTrue(!!caCert);

        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        // share the EAP TLS network
        share.checked = true;
        // trigger the onShareChanged_ event
        var event = new Event('change');
        share.dispatchEvent(event);
        // check that share is enabled
        assertTrue(share.checked);

        // check that client certificate selection is empty
        clientCert = networkConfig.$$('#userCert').$$('select').value;
        assertFalse(!!clientCert);
        // check that ca device-wide cert is still selected
        caCert = networkConfig.$$('#serverCa').$$('select').value;
        assertTrue(!!caCert);
      });
    });
  });
});
