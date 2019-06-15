// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_header_interactive_test', function() {
  /** @enum {string} */
  const TestNames = {
    FocusPrintOnReady: 'focus print on ready',
  };

  const suiteName = 'PrintHeaderInteractiveTest';

  suite(suiteName, function() {
    /** @type {?PrintPreviewHeaderElement} */
    let header = null;

    /** @override */
    setup(function() {
      loadTimeData.overrideValues({
        newPrintPreviewLayoutEnabled: false,
      });
      PolymerTest.clearBody();
      const model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      header = document.createElement('print-preview-header');
      header.settings = model.settings;
      header.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      header.state = print_preview.State.NOT_READY;
      header.firstLoad = true;
      test_util.fakeDataBind(model, header, 'settings');
      document.body.appendChild(header);
    });

    // Tests that the print button is automatically focused when the destination
    // is ready.
    test(assert(TestNames.FocusPrintOnReady), function() {
      const printButton = header.$$('.action-button');
      assertTrue(!!printButton);
      const whenFocusDone = test_util.eventToPromise('focus', printButton);

      // Simulate initialization finishing.
      header.state = print_preview.State.READY;
      return whenFocusDone;
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
