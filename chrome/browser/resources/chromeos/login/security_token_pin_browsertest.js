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
  let pinKeyboardContainer;
  let pinKeyboard;
  let progressElement;
  let pinInput;
  let inputField;
  let submitElement;

  setup(() => {
    securityTokenPin = document.createElement('security-token-pin');
    document.body.appendChild(securityTokenPin);
    securityTokenPin.parameters = DEFAULT_PARAMETERS;

    pinKeyboardContainer = securityTokenPin.$$('#pinKeyboardContainer');
    assert(pinKeyboardContainer);
    pinKeyboard = securityTokenPin.$$('#pinKeyboard');
    assert(pinKeyboard);
    progressElement = securityTokenPin.$$('#progress');
    assert(progressElement);
    pinInput = pinKeyboard.$$('#pinInput');
    assert(pinInput);
    inputField = pinInput.$$('input');
    assert(inputField);
    submitElement = securityTokenPin.$$('#submit');
    assert(submitElement);
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

  test('focus restores after progress animation', () => {
    // The PIN keyboard is displayed initially.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);

    // The PIN keyboard gets focused.
    securityTokenPin.focus();
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);

    // The user submits some value while keeping the focus on the input field.
    pinInput.value = '123';
    const enterEvent = new Event('keydown');
    enterEvent.keyCode = 13;
    pinInput.dispatchEvent(enterEvent);
    // The PIN keyboard is replaced by the animation UI.
    expectTrue(pinKeyboardContainer.hidden);
    expectFalse(progressElement.hidden);

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };
    // The PIN keyboard is shown again, replacing the animation UI.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);
    // The focus is on the input field.
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);
  });

  test('focus set after progress animation', () => {
    // The PIN keyboard is displayed initially.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);

    // The user submits some value using the "Submit" UI button.
    pinInput.value = '123';
    submitElement.click();
    // The PIN keyboard is replaced by the animation UI.
    expectTrue(pinKeyboardContainer.hidden);
    expectFalse(progressElement.hidden);

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };
    // The PIN keyboard is shown again, replacing the animation UI.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);
    // The focus is on the input field.
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);
  });

  mocha.run();
});
