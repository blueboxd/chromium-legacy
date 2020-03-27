// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_next_e2e_test_base.js',
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/assert_additions.js',
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/mock_feedback.js'
]);

/**
 * Test fixture for ChromeVox options page.
 */
ChromeVoxOptionsTest = class extends ChromeVoxNextE2ETest {
  constructor() {
    super();
    window.press = this.press;
  }

  runOnOptionsPage(callback) {
    const mockFeedback = this.createMockFeedback();
    chrome.automation.getDesktop((desktop) => {
      desktop.addEventListener(
          chrome.automation.EventType.LOAD_COMPLETE, (evt) => {
            if (evt.target.docUrl.indexOf('options/options.html') == -1 ||
                !evt.target.docLoaded) {
              return;
            }

            mockFeedback.expectSpeech('ChromeVox Options');
            callback(mockFeedback, evt);
          });
      CommandHandler.onCommand('showOptionsPage');
    });
  }

  /**
   * @return {!MockFeedback}
   */
  createMockFeedback() {
    const mockFeedback =
        new MockFeedback(this.newCallback(), this.newCallback.bind(this));
    mockFeedback.install();
    return mockFeedback;
  }

  press(keyCode, modifiers) {
    return function() {
      BackgroundKeyboardHandler.sendKeyPress(keyCode, modifiers);
    };
  }
};

TEST_F('ChromeVoxOptionsTest', 'DISABLED_NumberReadingStyleSelect', function() {
  this.runOnOptionsPage((mockFeedback, evt) => {
    const numberStyleSelect = evt.target.find({
      role: chrome.automation.RoleType.POP_UP_BUTTON,
      attributes: {name: 'Read numbers as:'}
    });
    assertNotNullNorUndefined(numberStyleSelect);
    mockFeedback.call(numberStyleSelect.focus.bind(numberStyleSelect))
        .expectSpeech('Read numbers as:', 'Words', 'Collapsed')
        .call(numberStyleSelect.doDefault.bind(numberStyleSelect))
        .expectSpeech('Expanded')

        // Before selecting the menu option.
        .call(() => {
          assertEquals('asWords', localStorage['numberReadingStyle']);
        })

        .call(press(40 /* ArrowDown */))
        .expectSpeech('Digits', 'Menu item', ' 2 of 2 ')
        .call(press(13 /* enter */))

        // TODO: The underlying select behavior here is unexpected because we
        // never get a new focus event for the select (moving us away from the
        // menu item). We simply repeat the menu item.
        .expectSpeech('Digits', 'Menu item', ' 2 of 2 ')
        .call(() => {
          assertEquals('asDigits', localStorage['numberReadingStyle']);
        })

        .replay();
  });
});

TEST_F('ChromeVoxOptionsTest', 'SmartStickyMode', function() {
  this.runOnOptionsPage((mockFeedback, evt) => {
    const smartStickyModeCheckbox = evt.target.find({
      role: chrome.automation.RoleType.CHECK_BOX,
      attributes:
          {name: 'Turn off sticky mode when editing text (Smart Sticky Mode)'}
    });
    assertNotNullNorUndefined(smartStickyModeCheckbox);
    mockFeedback
        .call(smartStickyModeCheckbox.focus.bind(smartStickyModeCheckbox))
        .expectSpeech(
            'Turn off sticky mode when editing text (Smart Sticky Mode)',
            'Check box', 'Checked')
        .call(() => {
          assertEquals('true', localStorage['smartStickyMode']);
        })
        .call(smartStickyModeCheckbox.doDefault.bind(smartStickyModeCheckbox))
        .expectSpeech(
            'Turn off sticky mode when editing text (Smart Sticky Mode)',
            'Check box', 'Not checked')
        .call(() => {
          assertEquals('false', localStorage['smartStickyMode']);
        })

        .replay();
  });
});
