// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('header_test', function() {
  /** @enum {string} */
  const TestNames = {
    HeaderPrinterTypes: 'header printer types',
    HeaderWithDuplex: 'header with duplex',
    HeaderWithCopies: 'header with copies',
    HeaderWithNup: 'header with nup',
    HeaderChangesForState: 'header changes for state',
    ButtonOrder: 'button order',
    EnterprisePolicy: 'enterprise policy',
  };

  const suiteName = 'HeaderTest';
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
      model.set('settings.duplex.available', true);
      model.set('settings.duplex.value', false);

      header.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      header.state = print_preview.State.READY;
      header.managed = false;
      test_util.fakeDataBind(model, header, 'settings');
      document.body.appendChild(header);
    });

    function setPdfDestination() {
      header.set(
          'destination',
          new print_preview.Destination(
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
              print_preview.DestinationType.LOCAL,
              print_preview.DestinationOrigin.LOCAL,
              loadTimeData.getString('printToPDF'),
              print_preview.DestinationConnectionStatus.ONLINE));
    }

    // Tests that the 4 different messages (non-virtual printer singular and
    // plural, virtual printer singular and plural) all show up as expected.
    test(assert(TestNames.HeaderPrinterTypes), function() {
      const summary = header.$$('.summary');
      assertEquals('Total: 1 sheet of paper', summary.textContent);
      header.setSetting('pages', [1, 2, 3]);
      assertEquals('Total: 3 sheets of paper', summary.textContent);
      setPdfDestination();
      assertEquals('Total: 3 pages', summary.textContent);
      header.setSetting('pages', [1]);
      assertEquals('Total: 1 page', summary.textContent);
      // Verify the chrome://print case of a zero length document does not show
      // the summary.
      header.setSetting('pages', []);
      assertEquals('', summary.textContent);
    });

    // Tests that the message is correctly adjusted with a duplex printer.
    test(assert(TestNames.HeaderWithDuplex), function() {
      const summary = header.$$('.summary');
      assertEquals('Total: 1 sheet of paper', summary.textContent);
      header.setSetting('pages', [1, 2, 3]);
      assertEquals('Total: 3 sheets of paper', summary.textContent);
      header.setSetting('duplex', true);
      assertEquals('Total: 2 sheets of paper', summary.textContent);
      header.setSetting('pages', [1, 2]);
      assertEquals('Total: 1 sheet of paper', summary.textContent);
    });

    // Tests that the message is correctly adjusted with multiple copies.
    test(assert(TestNames.HeaderWithCopies), function() {
      const summary = header.$$('.summary');
      assertEquals('Total: 1 sheet of paper', summary.textContent);
      header.setSetting('copies', 4);
      assertEquals('Total: 4 sheets of paper', summary.textContent);
      header.setSetting('duplex', true);
      assertEquals('Total: 4 sheets of paper', summary.textContent);
      header.setSetting('pages', [1, 2]);
      assertEquals('Total: 4 sheets of paper', summary.textContent);
      header.setSetting('duplex', false);
      assertEquals('Total: 8 sheets of paper', summary.textContent);
    });

    // Tests that the correct message is shown for non-READY states, and that
    // the print button is disabled appropriately.
    test(assert(TestNames.HeaderChangesForState), function() {
      const summary = header.$$('.summary');
      const printButton = header.$$('.action-button');
      assertEquals('Total: 1 sheet of paper', summary.textContent);
      assertFalse(printButton.disabled);

      header.state = print_preview.State.NOT_READY;
      assertEquals('', summary.textContent);
      assertTrue(printButton.disabled);

      header.state = print_preview.State.PRINTING;
      assertEquals(loadTimeData.getString('printing'), summary.textContent);
      assertTrue(printButton.disabled);
      setPdfDestination();
      assertEquals(loadTimeData.getString('saving'), summary.textContent);

      header.error = print_preview.Error.INVALID_TICKET;
      header.state = print_preview.State.ERROR;
      assertEquals('', summary.textContent);
      assertTrue(printButton.disabled);

      header.state = print_preview.State.READY;
      header.error = print_preview.Error.INVALID_PRINTER;
      header.state = print_preview.State.ERROR;
      assertEquals('', summary.textContent);
      assertTrue(printButton.disabled);

      const testError = 'Error printing to cloud print';
      header.cloudPrintErrorMessage = testError;
      header.error = print_preview.Error.CLOUD_PRINT_ERROR;
      header.state = print_preview.State.FATAL_ERROR;
      assertEquals(testError, summary.textContent);
      assertTrue(printButton.disabled);
    });

    // Tests that the buttons are in the correct order for different platforms.
    // See https://crbug.com/880562.
    test(assert(TestNames.ButtonOrder), function() {
      // Verify that there are only 2 buttons.
      assertEquals(2, header.shadowRoot.querySelectorAll('cr-button').length);

      const firstButton = header.$$('cr-button:first-child');
      const lastButton = header.$$('cr-button:last-child');
      const printButton = header.$$('cr-button.action-button');
      const cancelButton = header.$$('cr-button.cancel-button');

      if (cr.isWindows) {
        // On Windows, the print button is on the left.
        assertEquals(firstButton, printButton);
        assertEquals(lastButton, cancelButton);
      } else {
        assertEquals(firstButton, cancelButton);
        assertEquals(lastButton, printButton);
      }
    });

    // Tests that enterprise badge shows up if any setting is managed.
    test(assert(TestNames.EnterprisePolicy), function() {
      assertTrue(header.$$('iron-icon').hidden);
      header.managed = true;
      assertFalse(header.$$('iron-icon').hidden);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
