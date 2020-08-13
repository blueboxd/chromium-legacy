// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LanguagesMetricsProxy, LanguagesMetricsProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestLanguagesMetricsProxy} from './test_os_languages_metrics_proxy.m.js';
// #import {assertFalse, assertTrue} from '../../chai_assert.js';
// clang-format on

suite('input page', () => {
  /** @type {!SettingsInputPageElement} */
  let inputPage;
  /** @type {!settings.LanguagesMetricsProxy} */
  let metricsProxy;

  setup(() => {
    document.body.innerHTML = '';
    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(() => {
      // Sets up test metrics proxy.
      metricsProxy = new settings.TestLanguagesMetricsProxy();
      settings.LanguagesMetricsProxyImpl.instance_ = metricsProxy;

      inputPage = document.createElement('os-settings-input-page');
      inputPage.prefs = prefElement.prefs;
      document.body.appendChild(inputPage);
    });
  });

  suite('records metrics', () => {
    test('when deactivating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', true);
      inputPage.$$('#showImeMenu').click();
      Polymer.dom.flush();

      assertFalse(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when activating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', false);
      inputPage.$$('#showImeMenu').click();
      Polymer.dom.flush();

      assertTrue(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });
  });
});
