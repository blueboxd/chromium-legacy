// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {FakeInputMethodPrivate} from './fake_input_method_private.m.js';
// #import {FakeLanguageSettingsPrivate} from './fake_language_settings_private.m.js';
// #import {isChromeOS, isWindows} from 'chrome://resources/js/cr.m.js';
// #import {LanguagesBrowserProxy} from 'chrome://settings/lazy_load.js';
// #import {TestBrowserProxy} from '../test_browser_proxy.m.js';
// clang-format on

cr.define('settings', function() {
  /** @implements {settings.LanguagesBrowserProxy} */
  /* #export */ class TestLanguagesBrowserProxy extends TestBrowserProxy {
    constructor() {
      const methodNames = [];
      if (cr.isChromeOS || cr.isWindows) {
        methodNames.push(
            'getProspectiveUILanguage', 'setProspectiveUILanguage');
      }

      super(methodNames);

      /** @private {!LanguageSettingsPrivate} */
      this.languageSettingsPrivate_ =
          new settings.FakeLanguageSettingsPrivate();

      /** @private {!InputMethodPrivate} */
      this.inputMethodPrivate_ = /** @type{!InputMethodPrivate} */ (
          new settings.FakeInputMethodPrivate());
    }

    /** @override */
    getLanguageSettingsPrivate() {
      return this.languageSettingsPrivate_;
    }

    /** @param {!LanguageSettingsPrivate} languageSettingsPrivate */
    setLanguageSettingsPrivate(languageSettingsPrivate) {
      this.languageSettingsPrivate_ = languageSettingsPrivate;
    }
  }

  if (cr.isChromeOS || cr.isWindows) {
    /** @override */
    TestLanguagesBrowserProxy.prototype.getProspectiveUILanguage = function() {
      this.methodCalled('getProspectiveUILanguage');
      return Promise.resolve('en-US');
    };

    /** @override */
    TestLanguagesBrowserProxy.prototype.setProspectiveUILanguage = function(
        language) {
      this.methodCalled('setProspectiveUILanguage', language);
    };
  }

  if (cr.isChromeOS) {
    /** @override */
    TestLanguagesBrowserProxy.prototype.getInputMethodPrivate = function() {
      return this.inputMethodPrivate_;
    };
  }

  // #cr_define_end
  return {
    TestLanguagesBrowserProxy: TestLanguagesBrowserProxy,
  };
});
