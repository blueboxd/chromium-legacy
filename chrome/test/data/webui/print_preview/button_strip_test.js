// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('button_strip_test', function() {
  /** @enum {string} */
  const TestNames = {
    ButtonStripChangesForState: 'button strip changes for state',
    ButtonOrder: 'button order',
    ButtonStripFiresEvents: 'button strip fires events',
  };

  const suiteName = 'ButtonStripTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewButtonStripElement} */
    let buttonStrip = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      buttonStrip = document.createElement('print-preview-button-strip');

      buttonStrip.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      buttonStrip.state = print_preview.State.READY;
      document.body.appendChild(buttonStrip);
    });

    // Tests that the correct message is shown for non-READY states, and that
    // the print button is disabled appropriately.
    test(assert(TestNames.ButtonStripChangesForState), function() {
      const printButton = buttonStrip.$$('.action-button');
      assertFalse(printButton.disabled);

      buttonStrip.state = print_preview.State.NOT_READY;
      assertTrue(printButton.disabled);

      buttonStrip.state = print_preview.State.PRINTING;
      assertTrue(printButton.disabled);

      buttonStrip.state = print_preview.State.ERROR;
      assertTrue(printButton.disabled);

      buttonStrip.state = print_preview.State.FATAL_ERROR;
      assertTrue(printButton.disabled);
    });

    // Tests that the buttons are in the correct order for different platforms.
    // See https://crbug.com/880562.
    test(assert(TestNames.ButtonOrder), function() {
      // Verify that there are only 2 buttons.
      assertEquals(
          2, buttonStrip.shadowRoot.querySelectorAll('cr-button').length);

      const firstButton = buttonStrip.$$('cr-button:not(:last-child)');
      const lastButton = buttonStrip.$$('cr-button:last-child');
      const printButton = buttonStrip.$$('cr-button.action-button');
      const cancelButton = buttonStrip.$$('cr-button.cancel-button');

      if (cr.isWindows) {
        // On Windows, the print button is on the left.
        assertEquals(firstButton, printButton);
        assertEquals(lastButton, cancelButton);
      } else {
        assertEquals(firstButton, cancelButton);
        assertEquals(lastButton, printButton);
      }
    });

    // Tests that the button strip fires print-requested and cancel-requested
    // events.
    test(assert(TestNames.ButtonStripFiresEvents), function() {
      const printButton = buttonStrip.$$('cr-button.action-button');
      const cancelButton = buttonStrip.$$('cr-button.cancel-button');

      const whenPrintRequested =
          test_util.eventToPromise('print-requested', buttonStrip);
      printButton.click();
      return whenPrintRequested.then(() => {
        const whenCancelRequested =
            test_util.eventToPromise('cancel-requested', buttonStrip);
        cancelButton.click();
        return whenCancelRequested;
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
