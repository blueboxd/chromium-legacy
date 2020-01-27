// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_privacy_page', function() {
  /**
   * @param {!Element} element
   * @param {boolean} displayed
   */
  function assertVisible(element, displayed) {
    assertEquals(
        displayed, window.getComputedStyle(element)['display'] != 'none');
  }

  /** @implements {settings.ClearBrowsingDataBrowserProxy} */
  class TestClearBrowsingDataBrowserProxy extends TestBrowserProxy {
    constructor() {
      super(['initialize', 'clearBrowsingData', 'getInstalledApps']);

      /**
       * The promise to return from |clearBrowsingData|.
       * Allows testing code to test what happens after the call is made, and
       * before the browser responds.
       * @private {?Promise}
       */
      this.clearBrowsingDataPromise_ = null;

      /**
       * Response for |getInstalledApps|.
       * @private {!Array<!InstalledApp>}
       */
      this.installedApps_ = [];
    }

    /** @param {!Promise} promise */
    setClearBrowsingDataPromise(promise) {
      this.clearBrowsingDataPromise_ = promise;
    }

    /** @override */
    clearBrowsingData(dataTypes, timePeriod, installedApps) {
      this.methodCalled(
          'clearBrowsingData', [dataTypes, timePeriod, installedApps]);
      cr.webUIListenerCallback('browsing-data-removing', true);
      return this.clearBrowsingDataPromise_ !== null ?
          this.clearBrowsingDataPromise_ :
          Promise.resolve();
    }

    /** @param {!Array<!InstalledApp>} apps */
    setInstalledApps(apps) {
      this.installedApps_ = apps;
    }

    /** @override */
    getInstalledApps(timePeriod) {
      this.methodCalled('getInstalledApps');
      return Promise.resolve(this.installedApps_);
    }

    /** @override */
    initialize() {
      this.methodCalled('initialize');
      return Promise.resolve(false);
    }
  }

  function getClearBrowsingDataPrefs() {
    return {
      browser: {
        clear_data: {
          browsing_history: {
            key: 'browser.clear_data.browsing_history',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          browsing_history_basic: {
            key: 'browser.clear_data.browsing_history_basic',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cache: {
            key: 'browser.clear_data.cache',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cache_basic: {
            key: 'browser.clear_data.cache_basic',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cookies: {
            key: 'browser.clear_data.cookies',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cookies_basic: {
            key: 'browser.clear_data.cookies_basic',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          download_history: {
            key: 'browser.clear_data.download_history',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          hosted_apps_data: {
            key: 'browser.clear_data.hosted_apps_data',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          form_data: {
            key: 'browser.clear_data.form_data',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          passwords: {
            key: 'browser.clear_data.passwords',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          site_settings: {
            key: 'browser.clear_data.site_settings',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          time_period: {
            key: 'browser.clear_data.time_period',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          time_period_basic: {
            key: 'browser.clear_data.time_period_basic',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
        },
        last_clear_browsing_data_tab: {
          key: 'browser.last_clear_browsing_data_tab',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
      }
    };
  }

  function registerUMALoggingTests() {
    suite('PrivacyPageUMACheck', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {settings.TestMetricsBrowserProxy} */
      let testMetricsBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      setup(function() {
        testMetricsBrowserProxy = new TestMetricsBrowserProxy();
        settings.MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        const testSyncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = testSyncBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        page.prefs = {
          profile: {password_manager_leak_detection: {value: true}},
          signin: {
            allowed_on_next_startup:
                {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
          },
          safebrowsing:
              {enabled: {value: true}, scout_reporting_enabled: {value: true}},
        };
        document.body.appendChild(page);
      });

      teardown(function() {
        page.remove();
      });

      test('LogMangeCerfificatesClick', function() {
        page.$$('#manageCertificates').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_MANAGE_CERTIFICATES,
                  result);
            });
      });

      test('LogClearBrowsingClick', function() {
        page.$$('#clearBrowsingData').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_CLEAR_BROWSING_DATA,
                  result);
            });
      });

      test('LogDoNotTrackClick', function() {
        page.$$('#doNotTrack').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_DO_NOT_TRACK,
                  result);
            });
      });

      test('LogCanMakePaymentToggleClick', function() {
        page.$$('#canMakePaymentToggle').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_PAYMENT_METHOD,
                  result);
            });
      });

      test('LogSiteSettingsSubpageClick', function() {
        page.$$('#site-settings-subpage-trigger').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS,
                  result);
            });
      });

      test('LogSafeBrowsingToggleClick', function() {
        Polymer.dom.flush();
        page.$$('#safeBrowsingToggle').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_SAFE_BROWSING,
                  result);
            });
      });

      test('LogSafeBrowsingReportingToggleClick', function() {
        Polymer.dom.flush();
        page.$$('#safeBrowsingReportingToggle').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_IMPROVE_SECURITY,
                  result);
            });
      });
    });
  }

  function registerNativeCertificateManagerTests() {
    assert(cr.isMac || cr.isWindows);
    suite('NativeCertificateManager', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      setup(function() {
        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
      });

      teardown(function() {
        page.remove();
      });

      test('NativeCertificateManager', function() {
        page.$$('#manageCertificates').click();
        return testBrowserProxy.whenCalled('showManageSSLCertificates');
      });
    });
  }

  function registerPrivacyPageTests() {
    suite('PrivacyPage', function() {
      /** @type {SettingsPrivacyPageElement} */
      let page;

      setup(function() {
        const testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        const testSyncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = testSyncBrowserProxy;
        PolymerTest.clearBody();

        page = document.createElement('settings-privacy-page');
        page.prefs = {
          profile: {password_manager_leak_detection: {value: true}},
          signin: {
            allowed_on_next_startup:
                {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
          },
          safebrowsing:
              {enabled: {value: true}, scout_reporting_enabled: {value: true}},
        };
        document.body.appendChild(page);
        return testSyncBrowserProxy.whenCalled('getSyncStatus');
      });

      teardown(function() {
        page.remove();
      });

      test('showClearBrowsingDataDialog', function() {
        assertFalse(!!page.$$('settings-clear-browsing-data-dialog'));
        page.$$('#clearBrowsingData').click();
        Polymer.dom.flush();

        const dialog = page.$$('settings-clear-browsing-data-dialog');
        assertTrue(!!dialog);

        // Ensure that the dialog is fully opened before returning from this
        // test, otherwise asynchronous code run in attached() can cause flaky
        // errors.
        return test_util.whenAttributeIs(
            dialog.$$('#clearBrowsingDataDialog'), 'open', '');
      });

      test('safeBrowsingReportingToggle', function() {
        const safeBrowsingToggle = page.$.safeBrowsingToggle;
        const safeBrowsingReportingToggle = page.$.safeBrowsingReportingToggle;
        assertTrue(safeBrowsingToggle.checked);
        assertFalse(safeBrowsingReportingToggle.disabled);
        assertTrue(safeBrowsingReportingToggle.checked);
        safeBrowsingToggle.click();
        Polymer.dom.flush();

        assertFalse(safeBrowsingToggle.checked);
        assertTrue(safeBrowsingReportingToggle.disabled);
        assertFalse(safeBrowsingReportingToggle.checked);
        assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);
        safeBrowsingToggle.click();
        Polymer.dom.flush();

        assertTrue(safeBrowsingToggle.checked);
        assertFalse(safeBrowsingReportingToggle.disabled);
        assertTrue(safeBrowsingReportingToggle.checked);
      });
    });
  }

  function registerClearBrowsingDataTestsDesktop() {
    assert(!cr.isChromeOS);
    suite('ClearBrowsingDataDesktop', function() {
      /** @type {settings.TestClearBrowsingDataBrowserProxy} */
      let testBrowserProxy;

      /** @type {TestSyncBrowserProxy} */
      let testSyncBrowserProxy;

      /** @type {SettingsClearBrowsingDataDialogElement} */
      let element;

      setup(function() {
        testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
        settings.ClearBrowsingDataBrowserProxyImpl.instance_ = testBrowserProxy;
        testSyncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = testSyncBrowserProxy;
        PolymerTest.clearBody();
        element = document.createElement('settings-clear-browsing-data-dialog');
        element.set('prefs', getClearBrowsingDataPrefs());
        document.body.appendChild(element);
        return testBrowserProxy.whenCalled('initialize').then(() => {
          assertTrue(element.$$('#clearBrowsingDataDialog').open);
        });
      });

      teardown(function() {
        element.remove();
      });

      test('ClearBrowsingDataSyncAccountInfoDesktop', function() {
        // Not syncing: the footer is hidden.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: false,
          hasError: false,
        });
        Polymer.dom.flush();
        assertFalse(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));

        // Syncing: the footer is shown, with the normal sync info.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: false,
        });
        Polymer.dom.flush();
        assertTrue(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));
        assertVisible(element.$$('#sync-info'), true);
        assertVisible(element.$$('#sync-paused-info'), false);
        assertVisible(element.$$('#sync-passphrase-error-info'), false);
        assertVisible(element.$$('#sync-other-error-info'), false);

        // Sync is paused.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.REAUTHENTICATE,
        });
        Polymer.dom.flush();
        assertVisible(element.$$('#sync-info'), false);
        assertVisible(element.$$('#sync-paused-info'), true);
        assertVisible(element.$$('#sync-passphrase-error-info'), false);
        assertVisible(element.$$('#sync-other-error-info'), false);

        // Sync passphrase error.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.ENTER_PASSPHRASE,
        });
        Polymer.dom.flush();
        assertVisible(element.$$('#sync-info'), false);
        assertVisible(element.$$('#sync-paused-info'), false);
        assertVisible(element.$$('#sync-passphrase-error-info'), true);
        assertVisible(element.$$('#sync-other-error-info'), false);

        // Other sync error.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.NO_ACTION,
        });
        Polymer.dom.flush();
        assertVisible(element.$$('#sync-info'), false);
        assertVisible(element.$$('#sync-paused-info'), false);
        assertVisible(element.$$('#sync-passphrase-error-info'), false);
        assertVisible(element.$$('#sync-other-error-info'), true);
      });

      test('ClearBrowsingDataPauseSyncDesktop', function() {
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: false,
        });
        Polymer.dom.flush();
        assertTrue(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));
        const syncInfo = element.$$('#sync-info');
        assertVisible(syncInfo, true);
        const signoutLink = syncInfo.querySelector('a[href]');
        assertTrue(!!signoutLink);
        assertEquals(0, testSyncBrowserProxy.getCallCount('pauseSync'));
        signoutLink.click();
        assertEquals(1, testSyncBrowserProxy.getCallCount('pauseSync'));
      });

      test('ClearBrowsingDataStartSignInDesktop', function() {
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.REAUTHENTICATE,
        });
        Polymer.dom.flush();
        assertTrue(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));
        const syncInfo = element.$$('#sync-paused-info');
        assertVisible(syncInfo, true);
        const signinLink = syncInfo.querySelector('a[href]');
        assertTrue(!!signinLink);
        assertEquals(0, testSyncBrowserProxy.getCallCount('startSignIn'));
        signinLink.click();
        assertEquals(1, testSyncBrowserProxy.getCallCount('startSignIn'));
      });

      test('ClearBrowsingDataHandlePassphraseErrorDesktop', function() {
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.ENTER_PASSPHRASE,
        });
        Polymer.dom.flush();
        assertTrue(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));
        const syncInfo = element.$$('#sync-passphrase-error-info');
        assertVisible(syncInfo, true);
        const passphraseLink = syncInfo.querySelector('a[href]');
        assertTrue(!!passphraseLink);
        passphraseLink.click();
        assertEquals(
            settings.routes.SYNC,
            settings.Router.getInstance().getCurrentRoute());
      });
    });
  }

  function registerClearBrowsingDataTests() {
    suite('ClearBrowsingData', function() {
      /** @type {settings.TestClearBrowsingDataBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsClearBrowsingDataDialogElement} */
      let element;

      setup(function() {
        testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
        settings.ClearBrowsingDataBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        element = document.createElement('settings-clear-browsing-data-dialog');
        element.set('prefs', getClearBrowsingDataPrefs());
        document.body.appendChild(element);
        return testBrowserProxy.whenCalled('initialize');
      });

      teardown(function() {
        element.remove();
      });

      test('ClearBrowsingDataTap', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);
        assertFalse(element.$$('#installedAppsDialog').open);

        const cancelButton = element.$$('.cancel-button');
        assertTrue(!!cancelButton);
        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);
        const spinner = element.$$('paper-spinner-lite');
        assertTrue(!!spinner);

        // Select a datatype for deletion to enable the clear button.
        const cookieCheckbox = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckbox);
        cookieCheckbox.$.checkbox.click();

        assertFalse(cancelButton.disabled);
        assertFalse(actionButton.disabled);
        assertFalse(spinner.active);

        const promiseResolver = new PromiseResolver();
        testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
        actionButton.click();

        return testBrowserProxy.whenCalled('clearBrowsingData')
            .then(function(args) {
              const dataTypes = args[0];
              const timePeriod = args[1];
              const installedApps = args[2];
              assertEquals(1, dataTypes.length);
              assertEquals('browser.clear_data.cookies_basic', dataTypes[0]);
              assertTrue(element.$$('#clearBrowsingDataDialog').open);
              assertTrue(cancelButton.disabled);
              assertTrue(actionButton.disabled);
              assertTrue(spinner.active);
              assertTrue(installedApps.length == 0);

              // Simulate signal from browser indicating that clearing has
              // completed.
              cr.webUIListenerCallback('browsing-data-removing', false);
              promiseResolver.resolve();
              // Yields to the message loop to allow the callback chain of the
              // Promise that was just resolved to execute before the
              // assertions.
            })
            .then(function() {
              assertFalse(element.$$('#clearBrowsingDataDialog').open);
              assertFalse(cancelButton.disabled);
              assertFalse(actionButton.disabled);
              assertFalse(spinner.active);
              assertFalse(!!element.$$('#notice'));

              // Check that the dialog didn't switch to installed apps.
              assertFalse(element.$$('#installedAppsDialog').open);
            });
      });

      test('ClearBrowsingDataClearButton', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);
        const cookieCheckboxBasic = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckboxBasic);
        // Initially the button is disabled because all checkboxes are off.
        assertTrue(actionButton.disabled);
        // The button gets enabled if any checkbox is selected.
        cookieCheckboxBasic.$.checkbox.click();
        assertTrue(cookieCheckboxBasic.checked);
        assertFalse(actionButton.disabled);
        // Switching to advanced disables the button.
        element.$$('cr-tabs').selected = 1;
        assertTrue(actionButton.disabled);
        // Switching back enables it again.
        element.$$('cr-tabs').selected = 0;
        assertFalse(actionButton.disabled);
      });

      test('showHistoryDeletionDialog', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);
        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);

        // Select a datatype for deletion to enable the clear button.
        const cookieCheckbox = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckbox);
        cookieCheckbox.$.checkbox.click();
        assertFalse(actionButton.disabled);

        const promiseResolver = new PromiseResolver();
        testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
        actionButton.click();

        return testBrowserProxy.whenCalled('clearBrowsingData')
            .then(function() {
              // Passing showNotice = true should trigger the notice about other
              // forms of browsing history to open, and the dialog to stay open.
              promiseResolver.resolve(true /* showNotice */);

              // Yields to the message loop to allow the callback chain of the
              // Promise that was just resolved to execute before the
              // assertions.
            })
            .then(function() {
              Polymer.dom.flush();
              const notice = element.$$('#notice');
              assertTrue(!!notice);
              const noticeActionButton = notice.$$('.action-button');
              assertTrue(!!noticeActionButton);

              assertTrue(element.$$('#clearBrowsingDataDialog').open);
              assertTrue(notice.$$('#dialog').open);

              noticeActionButton.click();

              return new Promise(function(resolve, reject) {
                // Tapping the action button will close the notice. Move to the
                // end of the message loop to allow the closing event to
                // propagate to the parent dialog. The parent dialog should
                // subsequently close as well.
                setTimeout(function() {
                  const notice = element.$$('#notice');
                  assertFalse(!!notice);
                  assertFalse(element.$$('#clearBrowsingDataDialog').open);
                  resolve();
                }, 0);
              });
            });
      });

      test('Counters', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        const checkbox = element.$$('#cacheCheckboxBasic');
        assertEquals('browser.clear_data.cache_basic', checkbox.pref.key);

        // Simulate a browsing data counter result for history. This checkbox's
        // sublabel should be updated.
        cr.webUIListenerCallback(
            'update-counter-text', checkbox.pref.key, 'result');
        assertEquals('result', checkbox.subLabel);
      });

      test('history rows are hidden for supervised users', function() {
        assertFalse(loadTimeData.getBoolean('isSupervised'));
        assertFalse(element.$$('#browsingCheckbox').hidden);
        assertFalse(element.$$('#browsingCheckboxBasic').hidden);
        assertFalse(element.$$('#downloadCheckbox').hidden);

        element.remove();
        testBrowserProxy.reset();
        loadTimeData.overrideValues({isSupervised: true});

        element = document.createElement('settings-clear-browsing-data-dialog');
        document.body.appendChild(element);
        Polymer.dom.flush();

        return testBrowserProxy.whenCalled('initialize').then(function() {
          assertTrue(element.$$('#browsingCheckbox').hidden);
          assertTrue(element.$$('#browsingCheckboxBasic').hidden);
          assertTrue(element.$$('#downloadCheckbox').hidden);
        });
      });

      if (cr.isChromeOS) {
        // On ChromeOS the footer is never shown.
        test('ClearBrowsingDataSyncAccountInfo', function() {
          assertTrue(element.$$('#clearBrowsingDataDialog').open);

          // Not syncing.
          cr.webUIListenerCallback('sync-status-changed', {
            signedIn: false,
            hasError: false,
          });
          Polymer.dom.flush();
          assertFalse(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));

          // Syncing.
          cr.webUIListenerCallback('sync-status-changed', {
            signedIn: true,
            hasError: false,
          });
          Polymer.dom.flush();
          assertFalse(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));

          // Sync passphrase error.
          cr.webUIListenerCallback('sync-status-changed', {
            signedIn: true,
            hasError: true,
            statusAction: settings.StatusAction.ENTER_PASSPHRASE,
          });
          Polymer.dom.flush();
          assertFalse(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));

          // Other sync error.
          cr.webUIListenerCallback('sync-status-changed', {
            signedIn: true,
            hasError: true,
            statusAction: settings.StatusAction.NO_ACTION,
          });
          Polymer.dom.flush();
          assertFalse(!!element.$$('#clearBrowsingDataDialog [slot=footer]'));
        });
      }
    });
  }

  function registerPrivacyPageSoundTests() {
    suite('PrivacyPageSound', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      function flushAsync() {
        Polymer.dom.flush();
        return new Promise(resolve => {
          page.async(resolve);
        });
      }

      function getToggleElement() {
        return page.$$('settings-animated-pages')
            .queryEffectiveChildren('settings-subpage')
            .queryEffectiveChildren('#block-autoplay-setting');
      }

      setup(() => {
        loadTimeData.overrideValues({enableBlockAutoplayContentSetting: true});

        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();

        settings.Router.getInstance().navigateTo(
            settings.routes.SITE_SETTINGS_SOUND);
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
        return flushAsync();
      });

      teardown(() => {
        page.remove();
      });

      test('UpdateStatus', () => {
        assertTrue(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        cr.webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: true}, enabled: true});

        return flushAsync().then(() => {
          // Check that we are on and enabled.
          assertFalse(getToggleElement().hasAttribute('disabled'));
          assertTrue(getToggleElement().hasAttribute('checked'));

          // Toggle the pref off.
          cr.webUIListenerCallback(
              'onBlockAutoplayStatusChanged',
              {pref: {value: false}, enabled: true});

          return flushAsync().then(() => {
            // Check that we are off and enabled.
            assertFalse(getToggleElement().hasAttribute('disabled'));
            assertFalse(getToggleElement().hasAttribute('checked'));

            // Disable the autoplay status toggle.
            cr.webUIListenerCallback(
                'onBlockAutoplayStatusChanged',
                {pref: {value: false}, enabled: false});

            return flushAsync().then(() => {
              // Check that we are off and disabled.
              assertTrue(getToggleElement().hasAttribute('disabled'));
              assertFalse(getToggleElement().hasAttribute('checked'));
            });
          });
        });
      });

      test('Hidden', () => {
        assertTrue(
            loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
        assertFalse(getToggleElement().hidden);

        loadTimeData.overrideValues({enableBlockAutoplayContentSetting: false});

        page.remove();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);

        return flushAsync().then(() => {
          assertFalse(
              loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
          assertTrue(getToggleElement().hidden);
        });
      });

      test('Click', () => {
        assertTrue(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        cr.webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: true}, enabled: true});

        return flushAsync().then(() => {
          // Check that we are on and enabled.
          assertFalse(getToggleElement().hasAttribute('disabled'));
          assertTrue(getToggleElement().hasAttribute('checked'));

          // Click on the toggle and wait for the proxy to be called.
          getToggleElement().click();
          return testBrowserProxy.whenCalled('setBlockAutoplayEnabled')
              .then((enabled) => {
                assertFalse(enabled);
              });
        });
      });
    });
  }

  function registerInstalledAppsTests() {
    suite('InstalledApps', function() {
      /** @type {settings.TestClearBrowsingDataBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsClearBrowsingDataDialogElement} */
      let element;

      /** @type {Array<InstalledApp>} */
      const installedApps = [
        {registerableDomain: 'google.com', isChecked: true},
        {registerableDomain: 'yahoo.com', isChecked: true},
      ];

      setup(() => {
        loadTimeData.overrideValues({installedAppsInCbd: true});
        testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
        testBrowserProxy.setInstalledApps(installedApps);
        settings.ClearBrowsingDataBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        element = document.createElement('settings-clear-browsing-data-dialog');
        element.set('prefs', getClearBrowsingDataPrefs());
        document.body.appendChild(element);
        return testBrowserProxy.whenCalled('initialize');
      });

      teardown(() => {
        element.remove();
      });

      test('getInstalledApps', async function() {
        assertTrue(element.$.clearBrowsingDataDialog.open);
        assertFalse(element.$.installedAppsDialog.open);

        // Select cookie checkbox.
        element.$.cookiesCheckboxBasic.$.checkbox.click();
        assertTrue(element.$.cookiesCheckboxBasic.checked);
        // Clear browsing data.
        element.$.clearBrowsingDataConfirm.click();
        assertTrue(element.$.clearBrowsingDataDialog.open);

        await testBrowserProxy.whenCalled('getInstalledApps');
        await test_util.whenAttributeIs(
            element.$.installedAppsDialog, 'open', '');
        const firstInstalledApp = element.$$('installed-app-checkbox');
        assertTrue(!!firstInstalledApp);
        assertEquals(
            'google.com', firstInstalledApp.installed_app.registerableDomain);
        assertTrue(firstInstalledApp.installed_app.isChecked);
        // Choose to keep storage for google.com.
        firstInstalledApp.$.checkbox.click();
        assertFalse(firstInstalledApp.installed_app.isChecked);
        // Confirm deletion.
        element.$.installedAppsConfirm.click();
        const [dataTypes, timePeriod, apps] =
            await testBrowserProxy.whenCalled('clearBrowsingData');
        assertEquals(1, dataTypes.length);
        assertEquals('browser.clear_data.cookies_basic', dataTypes[0]);
        assertEquals(2, apps.length);
        assertEquals('google.com', apps[0].registerableDomain);
        assertFalse(apps[0].isChecked);
        assertEquals('yahoo.com', apps[1].registerableDomain);
        assertTrue(apps[1].isChecked);
      });
    });
  }

  return {
    registerNativeCertificateManagerTests,
    registerClearBrowsingDataTestsDesktop,
    registerClearBrowsingDataTests,
    registerInstalledAppsTests,
    registerPrivacyPageTests,
    registerPrivacyPageSoundTests,
    registerUMALoggingTests,
  };
});
