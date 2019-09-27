// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper function to verify that printers in |printerListEntries| that contain
 * |searchTerm| are not in |hiddenEntries|.
 * @param {!Element} printerEntryListTestElement
 * @param {string} searchTerm
 */
function verifyFilteredPrinters(printerEntryListTestElement, searchTerm) {
  const printerListEntries = Array.from(
      printerEntryListTestElement.$.printerEntryList.querySelectorAll(
          'settings-cups-printers-entry'));
  const hiddenEntries = Array.from(
      printerEntryListTestElement.$.printerEntryList.querySelectorAll(
          'settings-cups-printers-entry[hidden]'));

  for (let i = 0; i < printerListEntries.length; ++i) {
    const entry = printerListEntries[i];
    if (hiddenEntries.indexOf(entry) == -1) {
      assertTrue(
          entry.printerEntry.printerInfo.printerName.toLowerCase().includes(
              searchTerm.toLowerCase()));
    }
  }
}

/**
 * Helper function to verify that the actual visible printers match the
 * expected printer list.
 * @param {!Element} printerEntryListTestElement
 * @param {!Array<!PrinterListEntry>} expectedVisiblePrinters
 */
function verifyVisiblePrinters(
    printerEntryListTestElement, expectedVisiblePrinters) {
  const actualPrinterList = Array.from(
      printerEntryListTestElement.$.printerEntryList.querySelectorAll(
          'settings-cups-printers-entry:not([hidden])'));

  assertEquals(expectedVisiblePrinters.length, actualPrinterList.length);
  for (let i = 0; i < expectedVisiblePrinters.length; i++) {
    const expectedPrinter = expectedVisiblePrinters[i].printerInfo;
    const actualPrinter = actualPrinterList[i].printerEntry.printerInfo;

    assertEquals(actualPrinter.printerName, expectedPrinter.printerName);
    assertEquals(actualPrinter.printerAddress, expectedPrinter.printerAddress);
    assertEquals(actualPrinter.printerId, expectedPrinter.printerId);
  }
}

/**
 * Helper function to verify that printers are hidden accordingly if they do not
 * match the search query. Also checks if the no search results section is shown
 * when appropriate.
 * @param {!Element} printerEntryListTestElement
 * @param {!Array<!PrinterListEntry>} expectedVisiblePrinters
 * @param {string} searchTerm
 */
function verifySearchQueryResults(
    printerEntryListTestElement, expectedVisiblePrinters, searchTerm) {
  printerEntryListTestElement.searchTerm = searchTerm;

  Polymer.dom.flush();

  verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
  verifyFilteredPrinters(printerEntryListTestElement, searchTerm);

  if (expectedVisiblePrinters.length) {
    assertTrue(printerEntryListTestElement.$$('#no-search-results').hidden);
  } else {
    assertFalse(printerEntryListTestElement.$$('#no-search-results').hidden);
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

  /** @type {?Array<!PrinterListEntry>} */
  let printerList = [];

  /** @type {string} */
  let searchTerm = '';

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
    printerList = [];
    searchTerm = '';
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
        createPrinterListEntry('test1', 'address', '1', PrinterType.SAVED));
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('test2', 'address', '2', PrinterType.SAVED));
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('google', 'address', '3', PrinterType.SAVED));

    Polymer.dom.flush();

    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google', 'address', '3', PrinterType.SAVED),
      createPrinterListEntry('test1', 'address', '1', PrinterType.SAVED),
      createPrinterListEntry('test2', 'address', '2', PrinterType.SAVED)
    ]);

    searchTerm = 'google';
    // Set the search term and filter out the printers. Filtering "google"
    // should result in one visible entry and two hidden entries.
    verifySearchQueryResults(
        printerEntryListTestElement,
        [createPrinterListEntry('google', 'address', '3', PrinterType.SAVED)],
        searchTerm);

    // Change the search term and assert that entries are filtered correctly.
    // Filtering "test" should result in two visible entries and one hidden
    // entry.
    searchTerm = 'test';
    verifySearchQueryResults(
        printerEntryListTestElement,
        [
          createPrinterListEntry('test1', 'address', '1', PrinterType.SAVED),
          createPrinterListEntry('test2', 'address', '2', PrinterType.SAVED)
        ],
        searchTerm);

    // Add more printers and assert that they are correctly filtered.
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('test3', 'address', '4', PrinterType.SAVED));
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('google2', 'address', '5', PrinterType.SAVED));

    Polymer.dom.flush();

    verifySearchQueryResults(
        printerEntryListTestElement,
        [
          createPrinterListEntry('test1', 'address', '1', PrinterType.SAVED),
          createPrinterListEntry('test2', 'address', '2', PrinterType.SAVED),
          createPrinterListEntry('test3', 'address', '4', PrinterType.SAVED)
        ],
        searchTerm);
  });

  test('NoSearchFound', function() {
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('test1', 'address', '1', PrinterType.SAVED));
    printerEntryListTestElement.push(
        'printers',
        createPrinterListEntry('google', 'address', '2', PrinterType.SAVED));

    Polymer.dom.flush();

    searchTerm = 'google';
    // Set the search term and filter out the printers. Filtering "google"
    // should result in one visible entry and two hidden entries.
    verifySearchQueryResults(
        printerEntryListTestElement,
        [createPrinterListEntry('google', 'address', '2', PrinterType.SAVED)],
        searchTerm);

    // Change search term to something that has no matches.
    searchTerm = 'noSearchFound';
    verifySearchQueryResults(printerEntryListTestElement, [], searchTerm);

    // Change search term back to "google" and verify that the No search found
    // message is no longer there.
    searchTerm = 'google';
    verifySearchQueryResults(
        printerEntryListTestElement,
        [createPrinterListEntry('google', 'address', '2', PrinterType.SAVED)],
        searchTerm);
  });
});
