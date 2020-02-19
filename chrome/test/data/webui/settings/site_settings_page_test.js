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

        page.$$(`#${settings.ContentSettingsTypes.COOKIES}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_COOKIES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.GEOLOCATION}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_LOCATION,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.CAMERA}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_CAMERA,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.MIC}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_MICROPHONE,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.SENSORS}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_SENSORS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.NOTIFICATIONS}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_NOTIFICATIONS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.JAVASCRIPT}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_JAVASCRIPT,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.PLUGINS}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_FLASH,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.IMAGES}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_IMAGES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.POPUPS}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_POPUPS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.BACKGROUND_SYNC}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_BACKGROUND_SYNC,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.SOUND}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_SOUND,
            result);

        if (loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$(`#${settings.ContentSettingsTypes.ADS}`).click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_ADS,
              result);
        }

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS}`)
            .click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS}`)
            .click();
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

          page.$$(`#${settings.ContentSettingsTypes.PROTOCOL_HANDLERS}`)
              .click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_HANDLERS,
              result);
        }

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.MIDI_DEVICES}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions
                .PRIVACY_SITE_SETTINGS_MIDI_DEVICES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.ZOOM_LEVELS}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_ZOOM_LEVELS,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.USB_DEVICES}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_USB_DEVICES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.SERIAL_PORTS}`).click();
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

          page.$$(`#${settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE}`)
              .click();
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

        if (cr.isChromeOS) {
          page.$$(`#${settings.ContentSettingsTypes.PROTECTED_CONTENT}`)
              .click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions
                  .PRIVACY_SITE_SETTINGS_PROTECTED_CONTENT,
              result);
        }

        settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
        testBrowserProxy.reset();

        page.$$(`#${settings.ContentSettingsTypes.CLIPBOARD}`).click();
        result =
            await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_CLIPBOARD,
            result);

        if (loadTimeData.getBoolean('enablePaymentHandlerContentSetting')) {
          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$(`#${settings.ContentSettingsTypes.PAYMENT_HANDLER}`).click();
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

          page.$$(`#${settings.ContentSettingsTypes.MIXEDSCRIPT}`).click();
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

          page.$$(`#${settings.ContentSettingsTypes.BLUETOOTH_SCANNING}`)
              .click();
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

          page.$$(`#${settings.ContentSettingsTypes.AR}`).click();
          result =
              await testBrowserProxy.whenCalled('recordSettingsPageHistogram');
          assertEquals(
              settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_AR,
              result);


          settings.Router.getInstance().navigateTo(
              settings.routes.SITE_SETTINGS);
          testBrowserProxy.reset();

          page.$$(`#${settings.ContentSettingsTypes.VR}`).click();
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
