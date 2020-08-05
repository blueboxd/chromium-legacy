// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.m.js';
// #import {OsResetBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {LifetimeBrowserProxy, LifetimeBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestOsResetBrowserProxy} from './test_os_reset_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

cr.define('settings_reset_page', function() {
  /** @enum {string} */
  const TestNames = {
    PowerwashDialogAction: 'PowerwashDialogAction',
    PowerwashDialogOpenClose: 'PowerwashDialogOpenClose',
    PowerwashFocusDeepLink: 'PowerwashFocusDeepLink',
    PowerwashFocusDeepLinkNoFlag: 'PowerwashFocusDeepLinkNoFlag',
    PowerwashFocusDeepLinkWrongId: 'PowerwashFocusDeepLinkWrongId',
  };

  suite('DialogTests', function() {
    let resetPage = null;

    /** @type {!settings.ResetPageBrowserProxy} */
    let resetPageBrowserProxy = null;

    /** @type {!settings.LifetimeBrowserProxy} */
    let lifetimeBrowserProxy = null;

    setup(function() {
      lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
      settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;

      resetPageBrowserProxy = new reset_page.TestOsResetBrowserProxy();
      settings.OsResetBrowserProxyImpl.instance_ = resetPageBrowserProxy;

      PolymerTest.clearBody();
      resetPage = document.createElement('os-settings-reset-page');
      document.body.appendChild(resetPage);
      Polymer.dom.flush();
    });

    teardown(function() {
      settings.Router.getInstance().resetRouteForTesting();
      resetPage.remove();
    });

    /**
     * @param {function(SettingsPowerwashDialogElement):!Element}
     *     closeButtonFn A function that returns the button to be used for
     *     closing the dialog.
     * @return {!Promise}
     */
    function testOpenClosePowerwashDialog(closeButtonFn) {
      // Open powerwash dialog.
      assertTrue(!!resetPage);
      resetPage.$.powerwash.click();
      Polymer.dom.flush();
      const dialog = resetPage.$$('os-settings-powerwash-dialog');
      assertTrue(!!dialog);
      assertTrue(dialog.$.dialog.open);
      const onDialogClosed = new Promise(function(resolve, reject) {
        dialog.addEventListener('close', function() {
          assertFalse(dialog.$.dialog.open);
          resolve();
        });
      });

      closeButtonFn(dialog).click();
      return Promise.all([
        onDialogClosed,
        resetPageBrowserProxy.whenCalled('onPowerwashDialogShow'),
      ]);
    }

    /**
     * Navigates to the deep link provided by |settingId| and returns true if
     * the focused element is |deepLinkElement|.
     * @param {!Element} deepLinkElement
     * @param {!string} settingId
     * @returns {!boolean}
     */
    async function isDeepLinkFocusedForSettingId(deepLinkElement, settingId) {
      const params = new URLSearchParams;
      params.append('settingId', settingId);
      settings.Router.getInstance().navigateTo(
          settings.routes.OS_RESET, params);

      await test_util.waitAfterNextRender(deepLinkElement);
      return deepLinkElement === getDeepActiveElement();
    }

    // Tests that the powerwash dialog opens and closes correctly, and
    // that chrome.send calls are propagated as expected.
    test(TestNames.PowerwashDialogOpenClose, function() {
      // Test case where the 'cancel' button is clicked.
      return testOpenClosePowerwashDialog(function(dialog) {
        return dialog.$.cancel;
      });
    });

    // Tests that when powerwash is requested chrome.send calls are
    // propagated as expected.
    test(TestNames.PowerwashDialogAction, async () => {
      // Open powerwash dialog.
      resetPage.$.powerwash.click();
      Polymer.dom.flush();
      const dialog = resetPage.$$('os-settings-powerwash-dialog');
      assertTrue(!!dialog);
      dialog.$.powerwash.click();
      const requestTpmFirmwareUpdate =
          await lifetimeBrowserProxy.whenCalled('factoryReset');
      assertFalse(requestTpmFirmwareUpdate);
    });

    // Tests that when the route changes to one containing a deep link to
    // powerwash, powerwash is focused.
    test(TestNames.PowerwashFocusDeepLink, async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: true});
      assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));
      assertTrue(
          await isDeepLinkFocusedForSettingId(resetPage.$.powerwash, '1600'),
          'Powerwash should be focused for settingId=1600.');
    });

    // Tests that when the deep linking flag is disabled, no focusing of deep
    // links occurs.
    test(TestNames.PowerwashFocusDeepLinkNoFlag, async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: false});
      assertFalse(loadTimeData.getBoolean('isDeepLinkingEnabled'));
      assertFalse(
          await isDeepLinkFocusedForSettingId(resetPage.$.powerwash, '1600'),
          'Powerwash should not be focused with flag disabled.');
    });

    // Tests that when the route changes to one containing a deep link not equal
    // to powerwash, no focusing of powerwash occurs.
    test(TestNames.PowerwashFocusDeepLinkWrongId, async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: true});
      assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));
      assertFalse(
          await isDeepLinkFocusedForSettingId(resetPage.$.powerwash, '1234'),
          'Powerwash should not be focused for settingId=1234.');
    });
  });

  // #cr_define_end
  return {};
});
