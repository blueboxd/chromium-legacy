// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper function to verify that printers in |printerListEntries| that contain
 * |searchTerm| are not in |hiddenEntries|.
 * @param {!Element} printerListEntries
 * @param {!Element} hiddenEntries
 * @param {string} searchTerm
 */
function verifyFilteredPrinters(printerListEntries, hiddenEntries, searchTerm) {
  for (let i = 0; i < printerListEntries.length; ++i) {
    const entry = printerListEntries[i];
    if (hiddenEntries.indexOf(entry) == -1) {
      assertTrue(
          entry.printerEntry.printerInfo.printerName.toLowerCase().includes(
              searchTerm.toLowerCase()));
    }
  }
}

suite('CupsPrintersEntry', function() {
  /** A printer list entry created before each test. */
  let printerEntryTestElement = null;

  setup(function() {
    PolymerTest.clearBody();
    printerEntryTestElement =
        document.createElement('settings-cups-printers-entry');
    assertTrue(!!printerEntryTestElement);
    document.body.appendChild(printerEntryTestElement);
  });

  teardown(function() {
    printerEntryTestElement.remove();
    printerEntryTestElement = null;
  });

  test('initializePrinterEntry', function() {
    const expectedPrinterName = 'Test name';
    const expectedPrinterSubtext = 'Test subtext';

    printerEntryTestElement.printerEntry = {
      printerInfo: {
        ppdManufacturer: '',
        ppdModel: '',
        printerAddress: 'test:123',
        printerAutoconf: false,
        printerDescription: '',
        printerId: 'id_123',
        printerManufacturer: '',
        printerModel: '',
        printerMakeAndModel: '',
        printerName: expectedPrinterName,
        printerPPDPath: '',
        printerPpdReference: {
          userSuppliedPpdUrl: '',
          effectiveMakeAndModel: '',
          autoconf: false,
        },
        printerProtocol: 'ipp',
        printerQueue: 'moreinfohere',
        printerStatus: '',
      },
      printerType: PrinterType.SAVED,
    };
    assertEquals(
        expectedPrinterName,
        printerEntryTestElement.shadowRoot.querySelector('#printerName')
            .textContent.trim());

    assertTrue(
        printerEntryTestElement.shadowRoot.querySelector('#printerSubtext')
            .hidden);

    // Assert that setting the subtext will make it visible.
    printerEntryTestElement.subtext = expectedPrinterSubtext;
    assertFalse(
        printerEntryTestElement.shadowRoot.querySelector('#printerSubtext')
            .hidden);

    assertEquals(
        expectedPrinterSubtext,
        printerEntryTestElement.shadowRoot.querySelector('#printerSubtext')
            .textContent.trim());
  });

  test('savedPrinterShowsThreeDotMenu', function() {
    printerEntryTestElement.printerEntry = {
      printerInfo: {
        ppdManufacturer: '',
        ppdModel: '',
        printerAddress: 'test:123',
        printerAutoconf: false,
        printerDescription: '',
        printerId: 'id_123',
        printerManufacturer: '',
        printerModel: '',
        printerMakeAndModel: '',
        printerName: 'test1',
        printerPPDPath: '',
        printerPpdReference: {
          userSuppliedPpdUrl: '',
          effectiveMakeAndModel: '',
          autoconf: false,
        },
        printerProtocol: 'ipp',
        printerQueue: 'moreinfohere',
        printerStatus: '',
      },
      printerType: PrinterType.SAVED,
    };

    // Assert that three dot menu is not shown before the dom is updated.
    assertFalse(!!printerEntryTestElement.$$('.icon-more-vert'));

    Polymer.dom.flush();

    // Three dot menu should be visible when |printerType| is set to
    // PrinterType.SAVED.
    assertTrue(!!printerEntryTestElement.$$('.icon-more-vert'));
  });
});

suite('CupsPrinterEntryList', function() {
  /**
   * A printry entry list created before each test.
   * @type {PrintersEntryList}
   */
  let printerEntryListTestElement = null;

  setup(function() {
    PolymerTest.clearBody();
    printerEntryListTestElement =
        document.createElement('settings-cups-printers-entry-list');
    assertTrue(!!printerEntryListTestElement);
    document.body.appendChild(printerEntryListTestElement);
  });

  teardown(function() {
    printerEntryListTestElement.remove();
    printerEntryListTestElement = null;
  });

  /**
   * Helper function that creates a new PrinterListEntry.
   * @param {string} printerName
   * @param {string} printerAddress
   * @param {string} printerId
   * @param {string} printerType
   * @return {!PrinterListEntry}
   */
  function createPrinterListEntry(
      printerName, printerAddress, printerId, printerType) {
    const entry = {
      printerInfo: {
        ppdManufacturer: '',
        ppdModel: '',
        printerAddress: printerAddress,
        printerAutoconf: false,
        printerDescription: '',
        printerId: printerId,
        printerManufacturer: '',
        printerModel: '',
        printerMakeAndModel: '',
        printerName: printerName,
        printerPPDPath: '',
        printerPpdReference: {
          userSuppliedPpdUrl: '',
          effectiveMakeAndModel: '',
          autoconf: false,
        },
        printerProtocol: 'ipp',
        printerQueue: 'moreinfohere',
        printerStatus: '',
      },
      printerType: printerType,
    };
    return entry;
  }

  test('SearchTermFiltersCorrectPrinters', function() {
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('test1', '123', '1', PrinterType.SAVED));
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('test2', '123', '2', PrinterType.SAVED));
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('google', '123', '3', PrinterType.SAVED));

    Polymer.dom.flush();

    let printerListEntries =
        printerEntryListTestElement.shadowRoot.querySelectorAll(
            'settings-cups-printers-entry');

    assertEquals(3, printerListEntries.length);

    let searchTerm = 'google';

    // Set the search term and filter out the printers. Filtering "google"
    // should result in one visible entry and two hidden entries.
    printerEntryListTestElement.searchTerm = searchTerm;

    Polymer.dom.flush();

    let hiddenEntries = Array.from(
        printerEntryListTestElement.$.printerEntryList.querySelectorAll(
            'settings-cups-printers-entry[hidden]'));

    assertEquals(1, printerListEntries.length - hiddenEntries.length);
    verifyFilteredPrinters(printerListEntries, hiddenEntries, searchTerm);

    // Change the search term and assert that entries are filtered correctly.
    // Filtering "test" should result in two visible entries and one hidden
    // entry.
    searchTerm = 'test';
    printerEntryListTestElement.searchTerm = searchTerm;

    Polymer.dom.flush();

    hiddenEntries = Array.from(
        printerEntryListTestElement.$.printerEntryList.querySelectorAll(
            'settings-cups-printers-entry[hidden]'));

    assertEquals(2, printerListEntries.length - hiddenEntries.length);
    verifyFilteredPrinters(printerListEntries, hiddenEntries, searchTerm);

    // Add more printers and assert that they are correctly filtered.
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('test3', '123', '4', PrinterType.SAVED));
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('google2', '123', '5', PrinterType.SAVED));

    Polymer.dom.flush();

    hiddenEntries =
        Array.from(printerEntryListTestElement.shadowRoot.querySelectorAll(
            'settings-cups-printers-entry[hidden]'));

    assertEquals(3, printerListEntries.length - hiddenEntries.length);
    verifyFilteredPrinters(printerListEntries, hiddenEntries, searchTerm);
  });
});
