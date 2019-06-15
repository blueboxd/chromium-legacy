// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('button_strip_interactive_test', function() {
  /** @enum {string} */
  const TestNames = {
    FocusPrintOnReady: 'focus print on ready',
  };

  const suiteName = 'ButtonStripInteractiveTest';

  suite(suiteName, function() {
    /** @type {?PrintPreviewButtonStripElement} */
    let buttonStrip = null;

    /** @override */
    setup(function() {
      loadTimeData.overrideValues({
        newPrintPreviewLayoutEnabled: true,
      });
      PolymerTest.clearBody();

      buttonStrip = document.createElement('print-preview-button-strip');
      buttonStrip.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      buttonStrip.state = print_preview.State.NOT_READY;
      buttonStrip.firstLoad = true;
      document.body.appendChild(buttonStrip);
    });

    // Tests that the print button is automatically focused when the destination
    // is ready.
    test(assert(TestNames.FocusPrintOnReady), function() {
      const printButton = buttonStrip.$$('.action-button');
      assertTrue(!!printButton);
      const whenFocusDone = test_util.eventToPromise('focus', printButton);

      // Simulate initialization finishing.
      buttonStrip.state = print_preview.State.READY;
      return whenFocusDone;
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
