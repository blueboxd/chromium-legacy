// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_site_settings_page', function() {
  function registerUMALoggingTests() {
    suite('SiteSettingsPageUMACheck', function() {
      /** @type {settings.TestMetricsBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsSiteSettingsPageElement} */
      let page;

      setup(function() {
        testBrowserProxy = new TestMetricsBrowserProxy();
        settings.MetricsBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-site-settings-page');
        document.body.appendChild(page);
        Polymer.dom.flush();
      });

      teardown(function() {
        page.remove();
      });

      test('LogAllSiteSettingsPageClicks', async function() {
        page.$$('#allSites').click();
        let result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_ALL,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#cookies').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_COOKIES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#location').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_LOCATION,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#camera').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_CAMERA,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#microphone').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_MICROPHONE,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#sensors').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_SENSORS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#notifications').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_NOTIFICATIONS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#javascript').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_JAVASCRIPT,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#flash').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_FLASH,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#images').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_IMAGES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#popups').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_POPUPS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#backgroundSync').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_BACKGROUND_SYNC,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#sound').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_SOUND,
            result);

        if (loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$('#ads').click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_ADS,
              result);
        }

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#automaticDownloads').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#unsandboxedPlugins').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_UNSANDBOXED_PLUGINS,
            result);

        if (!loadTimeData.getBoolean('isGuest')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$('#protocolHandlers').click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_HANDLERS,
              result);
        }

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#midiDevices').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_MIDI_DEVICES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#zoomLevels').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_ZOOM_LEVELS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#usbDevices').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_USB_DEVICES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#serialPorts').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_SERIAL_PORTS,
            result);

        if (loadTimeData.getBoolean(
                'enableNativeFileSystemWriteContentSetting')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$('#nativeFileSystemWrite').click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions
                  .PRIVACY_SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE,
              result);
        }

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#pdfDocuments').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_PDF_DOCUMENTS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#protectedContent').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_PROTECTED_CONTENT,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$('#clipboard').click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_CLIPBOARD,
            result);

        if (loadTimeData.getBoolean('enablePaymentHandlerContentSetting')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$('#paymentHandler').click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions
                  .PRIVACY_SITE_SETTINGS_PAYMENT_HANDLER,
              result);
        }

        if (loadTimeData.getBoolean('enableInsecureContentContentSetting')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$('#mixedScript').click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions
                  .PRIVACY_SITE_SETTINGS_MIXEDSCRIPT,
              result);
        }

        if (loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$('#bluetoothScanning').click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions
                  .PRIVACY_SITE_SETTINGS_BLUETOOTH_SCANNING,
              result);
        }

        if (loadTimeData.getBoolean('enableWebXrContentSetting')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$('#ar').click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_AR,
              result);


          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$('#vr').click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_VR,
              result);
        }
      });
    });
  }
  return {
    registerUMALoggingTests,
  };
});
