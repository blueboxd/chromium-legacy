// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
let smartInputsPage = null;

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
});
