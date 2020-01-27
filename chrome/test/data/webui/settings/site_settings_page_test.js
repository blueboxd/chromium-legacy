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

      test('LogCookiesClick', function() {
        page.$$('#cookies').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_COOKIES,
                  result);
            });
      });

      test('LogLocationClick', function() {
        page.$$('#location').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_LOCATION,
                  result);
            });
      });

      test('LogCameraClick', function() {
        page.$$('#camera').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_CAMERA,
                  result);
            });
      });

      test('LogMicrophoneClick', function() {
        page.$$('#microphone').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_MICROPHONE,
                  result);
            });
      });

      test('LogSensorsClick', function() {
        page.$$('#sensors').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_SENSORS,
                  result);
            });
      });

      test('LogNotificationsClick', function() {
        page.$$('#notifications').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_NOTIFICATIONS,
                  result);
            });
      });
    });
  }

  // Split tests into 4 to prevent timeout.
  function registerUMALoggingTestsPart2() {
    suite('SiteSettingsPageUMACheckPart2', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
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

      test('LogJavascriptClick', function() {
        page.$$('#javascript').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_JAVASCRIPT,
                  result);
            });
      });

      test('LogFlashClick', function() {
        page.$$('#flash').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_FLASH,
                  result);
            });
      });

      test('LogImagesClick', function() {
        page.$$('#images').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_IMAGES,
                  result);
            });
      });

      test('LogPopupsClick', function() {
        page.$$('#popups').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_POPUPS,
                  result);
            });
      });

      if (loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter')) {
        test('LogAdsClick', function() {
          page.$$('#ads').click();
          return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
              .then(result => {
                assertEquals(
                    settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_ADS,
                    result);
              });
        });
      }

      test('LogBackgroundSyncClick', function() {
        page.$$('#background-sync').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_BACKGROUND_SYNC,
                  result);
            });
      });

      test('LogSoundClick', function() {
        page.$$('#sound').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS_SOUND,
                  result);
            });
      });
    });
  }

  // Split tests into 4 to prevent timeout.
  function registerUMALoggingTestsPart3() {
    suite('SiteSettingsPageUMACheckPart3', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
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

      test('LogAutomaticDownloadsClick', function() {
        page.$$('#automatic-downloads').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
                  result);
            });
      });

      test('LogUnsandboxedPluginsClick', function() {
        page.$$('#unsandboxed-plugins').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_UNSANDBOXED_PLUGINS,
                  result);
            });
      });

      if (!loadTimeData.getBoolean('isGuest')) {
        test('LogHandlersClick', function() {
          page.$$('#protocol-handlers').click();
          return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
              .then(result => {
                assertEquals(
                    settings.SettingsPageInteractions
                        .PRIVACY_SITE_SETTINGS_HANDLERS,
                    result);
              });
        });
      }

      test('LogMidiDevicesClick', function() {
        page.$$('#midi-devices').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_MIDI_DEVICES,
                  result);
            });
      });

      test('LogZoomLevelsClick', function() {
        page.$$('#zoom-levels').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_ZOOM_LEVELS,
                  result);
            });
      });

      test('LogUSBDevicesClick', function() {
        page.$$('#usb-devices').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_USB_DEVICES,
                  result);
            });
      });

      test('LogSerialPortsClick', function() {
        page.$$('#serial-ports').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_SERIAL_PORTS,
                  result);
            });
      });
    });
  }

  // Split tests into 4 to prevent timeout.
  function registerUMALoggingTestsPart4() {
    suite('SiteSettingsPageUMACheckPart4', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
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

      if (loadTimeData.getBoolean(
              'enableNativeFileSystemWriteContentSetting')) {
        test('LogNativeFileSystemWriteClick', function() {
          page.$$('#native-file-system-write').click();
          return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
              .then(result => {
                assertEquals(
                    settings.SettingsPageInteractions
                        .PRIVACY_SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE,
                    result);
              });
        });
      }

      test('LogPDFDocumentsClick', function() {
        page.$$('#pdf-documents').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_PDF_DOCUMENTS,
                  result);
            });
      });

      test('LogProtectedContentClick', function() {
        page.$$('#protected-content').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_PROTECTED_CONTENT,
                  result);
            });
      });

      test('LogClipboardClick', function() {
        page.$$('#clipboard').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions
                      .PRIVACY_SITE_SETTINGS_CLIPBOARD,
                  result);
            });
      });

      if (loadTimeData.getBoolean('enablePaymentHandlerContentSetting')) {
        test('LogPaymentHandlerClick', function() {
          page.$$('#paymentHandler').click();
          return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
              .then(result => {
                assertEquals(
                    settings.SettingsPageInteractions
                        .PRIVACY_SITE_SETTINGS_PAYMENT_HANDLER,
                    result);
              });
        });
      }

      if (loadTimeData.getBoolean('enableInsecureContentContentSetting')) {
        test('LogMixedscriptClick', function() {
          page.$$('#mixedscript').click();
          return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
              .then(result => {
                assertEquals(
                    settings.SettingsPageInteractions
                        .PRIVACY_SITE_SETTINGS_MIXEDSCRIPT,
                    result);
              });
        });
      }

      if (loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures')) {
        test('LogBluetoothScanningClick', function() {
          page.$$('#bluetooth-scanning').click();
          return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
              .then(result => {
                assertEquals(
                    settings.SettingsPageInteractions
                        .PRIVACY_SITE_SETTINGS_BLUETOOTH_SCANNING,
                    result);
              });
        });
      }
    });
  }
  return {
    registerUMALoggingTests,
    registerUMALoggingTestsPart2,
    registerUMALoggingTestsPart3,
    registerUMALoggingTestsPart4,
  };
});