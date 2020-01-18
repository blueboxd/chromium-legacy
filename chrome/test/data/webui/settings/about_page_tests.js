// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_about_page', function() {
  function setupRouter() {
    const routes = {
      BASIC: new settings.Route('/'),
      ABOUT: new settings.Route('/help'),
    };
    settings.Router.resetInstanceForTesting(new settings.Router(routes));
    settings.routes = routes;
  }

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
        loadTimeData.overrideValues({
          aboutObsoleteNowOrSoon: false,
          aboutObsoleteEndOfTheLine: false,
        });

        setupRouter();
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
        page = document.createElement('settings-about-page');
        settings.Router.getInstance().navigateTo(settings.routes.ABOUT);
        document.body.appendChild(page);
        return cr.isChromeOS ?
            Promise.resolve() :
            aboutBrowserProxy.whenCalled('refreshUpdateStatus');
      }

      if (!cr.isChromeOS) {
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

        /**
         * Test that when the current platform has been marked as deprecated
         * (but not end of the line) a deprecation warning message is displayed,
         * without interfering with the update status message and icon.
         */
        test('ObsoleteSystem', async () => {
          loadTimeData.overrideValues({
            aboutObsoleteNowOrSoon: true,
            aboutObsoleteEndOfTheLine: false,
          });

          await initNewPage();
          const icon = page.$$('iron-icon');
          assertTrue(!!icon);
          assertTrue(!!page.$.updateStatusMessage);
          assertTrue(!!page.$.deprecationWarning);
          assertFalse(page.$.deprecationWarning.hidden);

          fireStatusChanged(UpdateStatus.CHECKING);
          assertEquals(SPINNER_ICON, icon.src);
          assertEquals(null, icon.getAttribute('icon'));
          assertFalse(page.$.deprecationWarning.hidden);
          assertFalse(page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.UPDATING);
          assertEquals(SPINNER_ICON, icon.src);
          assertEquals(null, icon.getAttribute('icon'));
          assertFalse(page.$.deprecationWarning.hidden);
          assertFalse(page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
          assertEquals(null, icon.src);
          assertEquals('settings:check-circle', icon.icon);
          assertFalse(page.$.deprecationWarning.hidden);
          assertFalse(page.$.updateStatusMessage.hidden);
        });

        /**
         * Test that when the current platform has reached the end of the line,
         * a deprecation warning message and an error icon is displayed.
         */
        test('ObsoleteSystemEndOfLine', async () => {
          loadTimeData.overrideValues({
            aboutObsoleteNowOrSoon: true,
            aboutObsoleteEndOfTheLine: true,
          });
          await initNewPage();
          const icon = page.$$('iron-icon');
          assertTrue(!!icon);
          assertTrue(!!page.$.deprecationWarning);
          assertTrue(!!page.$.updateStatusMessage);

          assertFalse(page.$.deprecationWarning.hidden);
          assertFalse(page.$.deprecationWarning.hidden);

          fireStatusChanged(UpdateStatus.CHECKING);
          assertEquals(null, icon.src);
          assertEquals('cr:error', icon.icon);
          assertFalse(page.$.deprecationWarning.hidden);
          assertTrue(page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.FAILED);
          assertEquals(null, icon.src);
          assertEquals('cr:error', icon.icon);
          assertFalse(page.$.deprecationWarning.hidden);
          assertTrue(page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.UPDATED);
          assertEquals(null, icon.src);
          assertEquals('cr:error', icon.icon);
          assertFalse(page.$.deprecationWarning.hidden);
          assertTrue(page.$.updateStatusMessage.hidden);
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

        /*
         * Test that the "Relaunch" button updates according to incoming
         * 'update-status-changed' events.
         */
        test('ButtonsUpdate', function() {
          const relaunch = page.$.relaunch;
          assertTrue(!!relaunch);

          fireStatusChanged(UpdateStatus.CHECKING);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.UPDATING);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
          assertFalse(relaunch.hidden);

          fireStatusChanged(UpdateStatus.UPDATED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.FAILED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.DISABLED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
          assertTrue(relaunch.hidden);
        });
      }

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
        setupRouter();
        browserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-about-page');
        document.body.appendChild(page);
      });

      test('ReportAnIssue', function() {
        assertTrue(!!page.$.reportIssue);
        page.$.reportIssue.click();
        return browserProxy.whenCalled('openFeedbackDialog');
      });

      if (cr.isMac) {
        /**
         * A list of possible scenarios for the promoteUpdater.
         * @enum {!PromoteUpdaterStatus}
         */
        const PromoStatusScenarios = {
          CANT_PROMOTE: {
            hidden: true,
            disabled: true,
            actionable: false,
          },
          CAN_PROMOTE: {
            hidden: false,
            disabled: false,
            actionable: true,
          },
          IN_BETWEEN: {
            hidden: false,
            disabled: true,
            actionable: true,
          },
          PROMOTED: {
            hidden: false,
            disabled: true,
            actionable: false,
          },
        };

        /**
         * @param {!PromoteUpdaterStatus} status
         */
        function firePromoteUpdaterStatusChanged(status) {
          cr.webUIListenerCallback('promotion-state-changed', status);
        }

        /**
         * Tests that the button's states are wired up to the status correctly.
         */
        test('PromoteUpdaterButtonCorrectStates', function() {
          let item = page.$$('#promoteUpdater');
          let arrow = page.$$('#promoteUpdater cr-icon-button');
          assertFalse(!!item);
          assertFalse(!!arrow);

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CANT_PROMOTE);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          arrow = page.$$('#promoteUpdater cr-icon-button');
          assertFalse(!!item);
          assertFalse(!!arrow);

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
          Polymer.dom.flush();

          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertFalse(item.hasAttribute('disabled'));
          assertTrue(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater cr-icon-button');
          assertTrue(!!arrow);
          assertEquals('CR-ICON-BUTTON', arrow.parentElement.tagName);
          assertFalse(arrow.parentElement.hidden);
          assertFalse(arrow.hasAttribute('disabled'));

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.IN_BETWEEN);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertTrue(item.hasAttribute('disabled'));
          assertTrue(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater cr-icon-button');
          assertTrue(!!arrow);
          assertEquals('CR-ICON-BUTTON', arrow.parentElement.tagName);
          assertFalse(arrow.parentElement.hidden);
          assertTrue(arrow.hasAttribute('disabled'));

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.PROMOTED);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertTrue(item.hasAttribute('disabled'));
          assertFalse(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater cr-icon-button');
          assertTrue(!!arrow);
          assertEquals('CR-ICON-BUTTON', arrow.parentElement.tagName);
          assertTrue(arrow.parentElement.hidden);
          assertTrue(arrow.hasAttribute('disabled'));
        });

        test('PromoteUpdaterButtonWorksWhenEnabled', function() {
          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
          Polymer.dom.flush();
          const item = page.$$('#promoteUpdater');
          assertTrue(!!item);

          item.click();

          return browserProxy.whenCalled('promoteUpdater');
        });
      }
    });
  }

  return {
    registerTests: registerAboutPageTests,
    registerOfficialBuildTests: registerOfficialBuildTests,
  };
});
