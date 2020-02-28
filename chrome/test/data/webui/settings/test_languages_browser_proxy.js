// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {FakeInputMethodPrivate} from 'chrome://test/settings/fake_input_method_private.m.js';
// #import {FakeLanguageSettingsPrivate} from 'chrome://test/settings/fake_language_settings_private.m.js';
// #import {isChromeOS, isWindows} from 'chrome://resources/js/cr.m.js';
// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// clang-format on

cr.define('settings', function() {
  /** @implements {settings.LanguagesBrowserProxy} */
  /* #export */ class TestLanguagesBrowserProxy extends TestBrowserProxy {
    constructor() {
      const methodNames = [];
      if (cr.isChromeOS || cr.isWindows) {
        methodNames.push('getProspectiveUILanguage');
      }

      super(methodNames);

      /** @private {!LanguageSettingsPrivate} */
      this.languageSettingsPrivate_ =
          new settings.FakeLanguageSettingsPrivate();

      /** @private {!InputMethodPrivate} */
      this.inputMethodPrivate_ = new settings.FakeInputMethodPrivate();
    }

    /** @override */
    getLanguageSettingsPrivate() {
      return this.languageSettingsPrivate_;
    }

    /** @override */
    getInputMethodPrivate() {
      return this.inputMethodPrivate_;
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
  }

  // #cr_define_end
  return {
    TestLanguagesBrowserProxy: TestLanguagesBrowserProxy,
  };
});
