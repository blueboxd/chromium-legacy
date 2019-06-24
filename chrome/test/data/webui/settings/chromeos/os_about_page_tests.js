// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_about_page', function() {
  function registerAboutPageTests() {
    /**
     * @param {!UpdateStatus} status
     * @param {{
     *   progress: number|undefined,
     *   message: string|undefined
     * }} opt_options
     */
    function fireStatusChanged(status, opt_options) {
      const options = opt_options || {};
      cr.webUIListenerCallback('update-status-changed', {
        progress: options.progress === undefined ? 1 : options.progress,
        message: options.message,
        status: status,
      });
    }

    suite('AboutPageTest', function() {
      let page = null;

      /** @type {?settings.TestAboutPageBrowserProxy} */
      let aboutBrowserProxy = null;

      /** @type {?settings.TestLifetimeBrowserProxy} */
      let lifetimeBrowserProxy = null;

      const SPINNER_ICON = 'chrome://resources/images/throbber_small.svg';

      setup(function() {
        lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
        settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;

        aboutBrowserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = aboutBrowserProxy;
        return initNewPage();
      });

      teardown(function() {
        page.remove();
        page = null;
      });

      /** @return {!Promise} */
      function initNewPage() {
        aboutBrowserProxy.reset();
        lifetimeBrowserProxy.reset();
        PolymerTest.clearBody();
        page = document.createElement('os-settings-about-page');
        settings.navigateTo(settings.routes.ABOUT);
        document.body.appendChild(page);
        return Promise.all([
          aboutBrowserProxy.whenCalled('getChannelInfo'),
          aboutBrowserProxy.whenCalled('refreshUpdateStatus'),
          aboutBrowserProxy.whenCalled('refreshTPMFirmwareUpdateStatus'),
        ]);
      }

      /**
       * Test that the status icon and status message update according to
       * incoming 'update-status-changed' events.
       */
      test('IconAndMessageUpdates', function() {
        const icon = page.$$('iron-icon');
        assertTrue(!!icon);
        const statusMessageEl = page.$$('#updateStatusMessage div');
        let previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.CHECKING);
        assertEquals(SPINNER_ICON, icon.src);
        assertEquals(null, icon.getAttribute('icon'));
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.UPDATING, {progress: 0});
        assertEquals(SPINNER_ICON, icon.src);
        assertEquals(null, icon.getAttribute('icon'));
        assertFalse(statusMessageEl.textContent.includes('%'));
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.UPDATING, {progress: 1});
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        assertTrue(statusMessageEl.textContent.includes('%'));
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertEquals(null, icon.src);
        assertEquals('settings:check-circle', icon.icon);
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
        assertEquals(null, icon.src);
        assertEquals('cr20:domain', icon.icon);
        assertEquals(0, statusMessageEl.textContent.trim().length);

        fireStatusChanged(UpdateStatus.FAILED);
        assertEquals(null, icon.src);
        assertEquals('cr:error', icon.icon);
        assertEquals(0, statusMessageEl.textContent.trim().length);

        fireStatusChanged(UpdateStatus.DISABLED);
        assertEquals(null, icon.src);
        assertEquals(null, icon.getAttribute('icon'));
        assertEquals(0, statusMessageEl.textContent.trim().length);
      });

      test('ErrorMessageWithHtml', function() {
        const htmlError = 'hello<br>there<br>was<pre>an</pre>error';
        fireStatusChanged(UpdateStatus.FAILED, {message: htmlError});
        const statusMessageEl = page.$$('#updateStatusMessage div');
        assertEquals(htmlError, statusMessageEl.innerHTML);
      });

      test('FailedLearnMoreLink', function() {
        // Check that link is shown when update failed.
        fireStatusChanged(UpdateStatus.FAILED, {message: 'foo'});
        assertTrue(!!page.$$('#updateStatusMessage a:not([hidden])'));

        // Check that link is hidden when update hasn't failed.
        fireStatusChanged(UpdateStatus.UPDATED, {message: ''});
        assertTrue(!!page.$$('#updateStatusMessage a[hidden]'));
      });

      test('Relaunch', function() {
        let relaunch = page.$.relaunch;
        assertTrue(!!relaunch);
        assertTrue(relaunch.hidden);

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertFalse(relaunch.hidden);

        relaunch = page.$.relaunch;
        assertTrue(!!relaunch);
        relaunch.click();
        return lifetimeBrowserProxy.whenCalled('relaunch');
      });

      test('NoInternet', function() {
        assertTrue(page.$.updateStatusMessage.hidden);
        aboutBrowserProxy.sendStatusNoInternet();
        Polymer.dom.flush();
        assertFalse(page.$.updateStatusMessage.hidden);
        assertNotEquals(
            page.$.updateStatusMessage.innerHTML.includes('no internet'));
      });

      /**
       * Test that all buttons update according to incoming
       * 'update-status-changed' events for the case where target and current
       * channel are the same.
       */
      test('ButtonsUpdate_SameChannel', function() {
        const relaunch = page.$.relaunch;
        const checkForUpdates = page.$.checkForUpdates;
        const relaunchAndPowerwash = page.$.relaunchAndPowerwash;

        assertTrue(!!relaunch);
        assertTrue(!!relaunchAndPowerwash);
        assertTrue(!!checkForUpdates);

        function assertAllHidden() {
          assertTrue(checkForUpdates.hidden);
          assertTrue(relaunch.hidden);
          assertTrue(relaunchAndPowerwash.hidden);
          // Ensure that when all buttons are hidden, the container is also
          // hidden.
          assertTrue(page.$.buttonContainer.hidden);
        }

        // Check that |UPDATED| status is ignored if the user has not
        // explicitly checked for updates yet.
        fireStatusChanged(UpdateStatus.UPDATED);
        assertFalse(checkForUpdates.hidden);
        assertTrue(relaunch.hidden);
        assertTrue(relaunchAndPowerwash.hidden);

        // Check that the "Check for updates" button gets hidden for certain
        // UpdateStatus values, even if the CHECKING state was never
        // encountered (for example triggering update from crosh command
        // line).
        fireStatusChanged(UpdateStatus.UPDATING);
        assertAllHidden();
        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertTrue(checkForUpdates.hidden);
        assertFalse(relaunch.hidden);
        assertTrue(relaunchAndPowerwash.hidden);

        fireStatusChanged(UpdateStatus.CHECKING);
        assertAllHidden();

        fireStatusChanged(UpdateStatus.UPDATING);
        assertAllHidden();

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertTrue(checkForUpdates.hidden);
        assertFalse(relaunch.hidden);
        assertTrue(relaunchAndPowerwash.hidden);

        fireStatusChanged(UpdateStatus.UPDATED);
        assertAllHidden();

        fireStatusChanged(UpdateStatus.FAILED);
        assertFalse(checkForUpdates.hidden);
        assertTrue(relaunch.hidden);
        assertTrue(relaunchAndPowerwash.hidden);

        fireStatusChanged(UpdateStatus.DISABLED);
        assertAllHidden();

        fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
        assertAllHidden();
      });

      /**
       * Test that buttons update according to incoming
       * 'update-status-changed' events for the case where the target channel
       * is more stable than current channel.
       */
      test('ButtonsUpdate_BetaToStable', function() {
        aboutBrowserProxy.setChannels(
            BrowserChannel.BETA, BrowserChannel.STABLE);
        aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

        return initNewPage().then(function() {
          assertTrue(!!page.$.relaunch);
          assertTrue(!!page.$.relaunchAndPowerwash);

          assertTrue(page.$.relaunch.hidden);
          assertFalse(page.$.relaunchAndPowerwash.hidden);

          page.$.relaunchAndPowerwash.click();
          return lifetimeBrowserProxy.whenCalled('factoryReset')
              .then((requestTpmFirmwareUpdate) => {
                assertFalse(requestTpmFirmwareUpdate);
              });
        });
      });

      /**
       * Test that buttons update according to incoming
       * 'update-status-changed' events for the case where the target channel
       * is less stable than current channel.
       */
      test('ButtonsUpdate_StableToBeta', function() {
        aboutBrowserProxy.setChannels(
            BrowserChannel.STABLE, BrowserChannel.BETA);
        aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

        return initNewPage().then(function() {
          assertTrue(!!page.$.relaunch);
          assertTrue(!!page.$.relaunchAndPowerwash);

          assertFalse(page.$.relaunch.hidden);
          assertTrue(page.$.relaunchAndPowerwash.hidden);

          page.$.relaunch.click();
          return lifetimeBrowserProxy.whenCalled('relaunch');
        });
      });

      /**
       * Test that buttons update as a result of receiving a
       * 'target-channel-changed' event (normally fired from
       * <settings-channel-switcher-dialog>).
       */
      test('ButtonsUpdate_TargetChannelChangedEvent', function() {
        aboutBrowserProxy.setChannels(BrowserChannel.BETA, BrowserChannel.BETA);
        aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

        return initNewPage().then(function() {
          assertFalse(page.$.relaunch.hidden);
          assertTrue(page.$.relaunchAndPowerwash.hidden);

          page.fire('target-channel-changed', BrowserChannel.DEV);
          assertFalse(page.$.relaunch.hidden);
          assertTrue(page.$.relaunchAndPowerwash.hidden);

          page.fire('target-channel-changed', BrowserChannel.STABLE);
          assertTrue(page.$.relaunch.hidden);
          assertFalse(page.$.relaunchAndPowerwash.hidden);
        });
      });

      test('RegulatoryInfo', function() {
        let regulatoryInfo = null;

        /**
         * Checks the visibility of the "regulatory info" section.
         * @param {boolean} isShowing Whether the section is expected to be
         *     visible.
         * @return {!Promise}
         */
        function checkRegulatoryInfo(isShowing) {
          return aboutBrowserProxy.whenCalled('getRegulatoryInfo')
              .then(function() {
                const regulatoryInfoEl = page.$.regulatoryInfo;
                assertTrue(!!regulatoryInfoEl);
                assertEquals(isShowing, !regulatoryInfoEl.hidden);

                if (isShowing) {
                  const img = regulatoryInfoEl.querySelector('img');
                  assertTrue(!!img);
                  assertEquals(regulatoryInfo.text, img.getAttribute('alt'));
                  assertEquals(regulatoryInfo.url, img.getAttribute('src'));
                }
              });
        }

        return checkRegulatoryInfo(false)
            .then(function() {
              regulatoryInfo = {text: 'foo', url: 'bar'};
              aboutBrowserProxy.setRegulatoryInfo(regulatoryInfo);
              return initNewPage();
            })
            .then(function() {
              return checkRegulatoryInfo(true);
            });
      });

      test('TPMFirmwareUpdate', function() {
        return initNewPage()
            .then(function() {
              assertTrue(page.$.aboutTPMFirmwareUpdate.hidden);
              aboutBrowserProxy.setTPMFirmwareUpdateStatus(
                  {updateAvailable: true});
              aboutBrowserProxy.refreshTPMFirmwareUpdateStatus();
            })
            .then(function() {
              assertFalse(page.$.aboutTPMFirmwareUpdate.hidden);
              page.$.aboutTPMFirmwareUpdate.click();
            })
            .then(function() {
              const dialog = page.$$('os-settings-powerwash-dialog');
              assertTrue(!!dialog);
              assertTrue(dialog.$.dialog.open);
              dialog.$$('#powerwash').click();
              return lifetimeBrowserProxy.whenCalled('factoryReset')
                  .then((requestTpmFirmwareUpdate) => {
                    assertTrue(requestTpmFirmwareUpdate);
                  });
            });
      });

      test('DeviceEndOfLife', function() {
        /**
         * Checks the visibility of the end of life message and icon.
         * @param {boolean} isShowing Whether the end of life UI is expected
         *     to be visible.
         * @return {!Promise}
         */
        function checkHasEndOfLife(isShowing) {
          return aboutBrowserProxy.whenCalled('getHasEndOfLife')
              .then(function() {
                const endOfLifeMessageContainer =
                    page.$.endOfLifeMessageContainer;
                assertTrue(!!endOfLifeMessageContainer);
                assertEquals(isShowing, !endOfLifeMessageContainer.hidden);

                // Update status message should be hidden before user has
                // checked for updates.
                assertTrue(page.$.updateStatusMessage.hidden);

                fireStatusChanged(UpdateStatus.CHECKING);
                assertEquals(isShowing, page.$.updateStatusMessage.hidden);

                if (isShowing) {
                  const icon = page.$$('iron-icon');
                  assertTrue(!!icon);
                  assertEquals(null, icon.src);
                  assertEquals('settings:end-of-life', icon.icon);

                  const checkForUpdates = page.$.checkForUpdates;
                  assertTrue(!!checkForUpdates);
                  assertTrue(checkForUpdates.hidden);
                }
              });
        }

        // Force test proxy to not respond to JS requests.
        // End of life message should still be hidden in this case.
        aboutBrowserProxy.setHasEndOfLife(new Promise(function(res, rej) {}));
        return initNewPage()
            .then(function() {
              return checkHasEndOfLife(false);
            })
            .then(function() {
              aboutBrowserProxy.setHasEndOfLife(true);
              return initNewPage();
            })
            .then(function() {
              return checkHasEndOfLife(true);
            })
            .then(function() {
              aboutBrowserProxy.setHasEndOfLife(false);
              return initNewPage();
            })
            .then(function() {
              return checkHasEndOfLife(false);
            });
      });

      test('GetHelp', function() {
        assertTrue(!!page.$.help);
        page.$.help.click();
        return aboutBrowserProxy.whenCalled('openHelpPage');
      });
    });
  }

  function registerOfficialBuildTests() {
    suite('AboutPageTest_OfficialBuild', function() {
      let page = null;
      let browserProxy = null;

      setup(function() {
        browserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('os-settings-about-page');
        document.body.appendChild(page);
      });

      test('ReportAnIssue', function() {
        assertTrue(!!page.$.reportIssue);
        page.$.reportIssue.click();
        return browserProxy.whenCalled('openFeedbackDialog');
      });
    });
  }

  return {
    // TODO(aee): move the detailed build info and channel switch dialog tests
    // from the browser about page tests when those CrOS-specific parts are
    // removed from the browser about page.
    registerTests: registerAboutPageTests,
    registerOfficialBuildTests: registerOfficialBuildTests,
  };
});
