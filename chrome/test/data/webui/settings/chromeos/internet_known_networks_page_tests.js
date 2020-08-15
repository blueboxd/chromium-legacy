// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('InternetKnownNetworksPage', function() {
  /** @type {?SettingsInternetKnownNetworksPageElement} */
  let internetKnownNetworksPage = null;

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
      internetAddWiFi: 'internetAddWiFi',
      internetDetailPageTitle: 'internetDetailPageTitle',
      internetKnownNetworksPageTitle: 'internetKnownNetworksPageTitle',
    });

    mojoApi_ = new FakeNetworkConfig();
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
    mojoApi_.resetForTest();
    mojoApi_.addNetworksForTest(networks);
  }

  setup(function() {
    PolymerTest.clearBody();
    internetKnownNetworksPage =
        document.createElement('settings-internet-known-networks-page');
    assertTrue(!!internetKnownNetworksPage);
    mojoApi_.resetForTest();
    document.body.appendChild(internetKnownNetworksPage);
    return flushAsync();
  });

  teardown(function() {
    internetKnownNetworksPage.remove();
    internetKnownNetworksPage = null;
    settings.Router.getInstance().resetRouteForTesting();
  });

  suite('KnownNetworksPage', function() {
    test('WiFi', async () => {
      const mojom = chromeos.networkConfig.mojom;
      internetKnownNetworksPage.networkType = mojom.NetworkType.kWiFi;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      const preferredWifi =
          OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1');
      setNetworksForTest(mojom.NetworkType.kWiFi, [
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams;
      params.append('settingId', '7');
      settings.Router.getInstance().navigateTo(
          settings.routes.KNOWN_NETWORKS, params);

      await flushAsync();

      assertEquals(2, internetKnownNetworksPage.networkStateList_.length);

      const preferredList =
          internetKnownNetworksPage.$$('#preferredNetworkList');
      assertTrue(!!preferredList);
      const preferredElems = preferredList.querySelectorAll('cr-link-row');
      assertEquals(preferredElems.length, 1);

      const deepLinkElement = preferredElems[0].$$('#icon');
      assert(!!deepLinkElement);
      await test_util.waitAfterNextRender();
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Preferred list elem should be focused for settingId=7.');
    });
  });
});
