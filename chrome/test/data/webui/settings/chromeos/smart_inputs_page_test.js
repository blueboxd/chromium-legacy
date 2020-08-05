// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

let smartInputsPage;

function createSmartInputsPage() {
  PolymerTest.clearBody();
  smartInputsPage = document.createElement('os-settings-smart-inputs-page');
  document.body.appendChild(smartInputsPage);
  Polymer.dom.flush();
}

suite('SmartInputsPage', function() {
  teardown(function() {
    smartInputsPage.remove();
  });

  test(
      'assistPersonalInfoNotNullWhenAllowAssistivePersonalInfoIsTrue',
      function() {
        loadTimeData.overrideValues({allowAssistivePersonalInfo: true});
        createSmartInputsPage();
        assertTrue(!!smartInputsPage.$$('#assistPersonalInfo'));
      });

  test(
      'assistPersonalInfoNullWhenAllowAssistivePersonalInfoIsFalse',
      function() {
        loadTimeData.overrideValues({allowAssistivePersonalInfo: false});
        createSmartInputsPage();
        assertFalse(!!smartInputsPage.$$('#assistPersonalInfo'));
      });

  test('emojiSuggestAdditionNotNullWhenAllowEmojiSuggestionIsTrue', function() {
    loadTimeData.overrideValues({allowEmojiSuggestion: true});
    createSmartInputsPage();
    assertTrue(!!smartInputsPage.$$('#emojiSuggestion'));
  });

  test('emojiSuggestAdditionNullWhenAllowEmojiSuggestionIsFalse', function() {
    loadTimeData.overrideValues({allowEmojiSuggestion: false});
    createSmartInputsPage();
    assertFalse(!!smartInputsPage.$$('#emojiSuggestion'));
  });
});
