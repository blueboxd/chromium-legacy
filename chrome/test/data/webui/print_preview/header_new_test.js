// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('header_new_test', function() {
  /** @enum {string} */
  const TestNames = {
    HeaderPrinterTypes: 'header printer types',
    HeaderWithDuplex: 'header with duplex',
    HeaderWithCopies: 'header with copies',
    HeaderWithNup: 'header with nup',
    HeaderChangesForState: 'header changes for state',
    EnterprisePolicy: 'enterprise policy',
  };

  const suiteName = 'HeaderNewTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewHeaderNewElement} */
    let header = null;

    /** @override */
    setup(function() {
      loadTimeData.overrideValues({
        newPrintPreviewLayoutEnabled: true,
      });
      PolymerTest.clearBody();
      const model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      header = document.createElement('print-preview-header-new');
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
      assertEquals('1 sheet of paper', summary.textContent.trim());
      header.setSetting('pages', [1, 2, 3]);
      assertEquals('3 sheets of paper', summary.textContent.trim());
      setPdfDestination();
      assertEquals('3 pages', summary.textContent.trim());
      header.setSetting('pages', [1]);
      assertEquals('1 page', summary.textContent.trim());
      // Verify the chrome://print case of a zero length document does not show
      // the summary.
      header.setSetting('pages', []);
      assertEquals('', summary.textContent);
    });

    // Tests that the message is correctly adjusted with a duplex printer.
    test(assert(TestNames.HeaderWithDuplex), function() {
      const summary = header.$$('.summary');
      assertEquals('1 sheet of paper', summary.textContent.trim());
      header.setSetting('pages', [1, 2, 3]);
      assertEquals('3 sheets of paper', summary.textContent.trim());
      header.setSetting('duplex', true);
      assertEquals('2 sheets of paper', summary.textContent.trim());
      header.setSetting('pages', [1, 2]);
      assertEquals('1 sheet of paper', summary.textContent.trim());
    });

    // Tests that the message is correctly adjusted with multiple copies.
    test(assert(TestNames.HeaderWithCopies), function() {
      const summary = header.$$('.summary');
      assertEquals('1 sheet of paper', summary.textContent.trim());
      header.setSetting('copies', 4);
      assertEquals('4 sheets of paper', summary.textContent.trim());
      header.setSetting('duplex', true);
      assertEquals('4 sheets of paper', summary.textContent.trim());
      header.setSetting('pages', [1, 2]);
      assertEquals('4 sheets of paper', summary.textContent.trim());
      header.setSetting('duplex', false);
      assertEquals('8 sheets of paper', summary.textContent.trim());
    });

    // Tests that the correct message is shown for non-READY states, and that
    // the print button is disabled appropriately.
    test(assert(TestNames.HeaderChangesForState), function() {
      const summary = header.$$('.summary');
      assertEquals('1 sheet of paper', summary.textContent.trim());

      header.state = print_preview.State.NOT_READY;
      assertEquals('', summary.textContent.trim());

      header.state = print_preview.State.PRINTING;
      assertEquals(
          loadTimeData.getString('printing'), summary.textContent.trim());
      setPdfDestination();
      assertEquals(
          loadTimeData.getString('saving'), summary.textContent.trim());

      header.state = print_preview.State.ERROR;
      assertEquals('', summary.textContent.trim());

      const testError = 'Error printing to cloud print';
      header.cloudPrintErrorMessage = testError;
      header.error = print_preview.Error.CLOUD_PRINT_ERROR;
      header.state = print_preview.State.FATAL_ERROR;
      assertEquals(testError, summary.textContent.trim());
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
