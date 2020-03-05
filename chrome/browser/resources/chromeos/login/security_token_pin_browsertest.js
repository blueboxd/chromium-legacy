// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the <security-token-pin> Polymer element.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);

var PolymerSecurityTokenPinTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://oobe/login';
  }
};

TEST_F('PolymerSecurityTokenPinTest', 'All', function() {
  const DEFAULT_PARAMETERS = {
    codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
    enableUserInput: true,
    errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.NONE,
    attemptsLeft: -1
  };

  let securityTokenPin;
  let inputField;

  setup(() => {
    securityTokenPin = document.createElement('security-token-pin');
    document.body.appendChild(securityTokenPin);
    securityTokenPin.parameters = DEFAULT_PARAMETERS;

    inputField =
        securityTokenPin.$$('#pinKeyboard').$$('#pinInput').$$('input');
    assert(inputField);
  });

  // Test that no scrolling is necessary in order to see all dots after entering
  // a PIN of a typical length.
  test('8-digit PIN fits into input', () => {
    const PIN_LENGTH = 8;
    inputField.value = '0'.repeat(PIN_LENGTH);
    expectGT(inputField.scrollWidth, 0);
    expectLE(inputField.scrollWidth, inputField.clientWidth);
  });

  // Test that the distance between characters (dots) is set in a correct way
  // and doesn't fall back to the default value.
  test('PIN input letter-spacing is correctly set up', () => {
    expectNotEquals(
        getComputedStyle(inputField).getPropertyValue('letter-spacing'),
        'normal');
  });

  mocha.run();
});
