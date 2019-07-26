// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickThreeDotMenu(printerEntry) {
  // Click on three dot menu on an item entry.
  const threeDot = printerEntry.$$('.icon-more-vert');
  threeDot.click();

  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickAddAutomaticButton(printerEntry) {
  // Click on add button on an item entry.
  const addButton = printerEntry.$$('.icon-add-circle');
  assertTrue(!!addButton);
  addButton.click();
  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickConfigureButton(printerEntry) {
  // Click on configure button on an item entry.
  const configureButton = printerEntry.$$('.action-button');
  assertTrue(!!configureButton);
  configureButton.click();
  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} dialog
 * @private
 */
function clickSaveButton(dialog) {
  const saveButton = dialog.$$('.action-button');

  assertTrue(!!saveButton);
  assertTrue(!saveButton.disabled);

  saveButton.click();

  Polymer.dom.flush();
}

/**
 * Helper method to pull an array of CupsPrinterEntry out of a
 * |printersElement|.
 * @param {!HTMLElement} printersElement
 * @return {!Array<!HTMLElement>}
 * @private
 */
function getPrinterEntries(printersElement) {
  // List component contained by |printersElement|.
  const listElement = printersElement.$$('settings-cups-printers-entry-list');

  const entryList = listElement.$$('#printerEntryList');
  return entryList.querySelectorAll('settings-cups-printers-entry');
}

/**
 * @param {!HTMLElement} page
 * @return {!HTMLElement}
 * @private
 */
function initializeEditDialog(page) {
  const editDialog = page.$$('#editPrinterDialog');
  assertTrue(!!editDialog);

  // Edit dialog will reset |ppdModel| when |ppdManufacturer| is set. Must
  // set values separately.
  editDialog.pendingPrinter_.ppdManufacturer = 'make';
  editDialog.pendingPrinter_.ppdModel = 'model';

  // Initializing |activePrinter| in edit dialog will set
  // |needsReconfigured_| to true. Reset it so that any changes afterwards
  // mimics user input.
  editDialog.needsReconfigured_ = false;

  return editDialog;
}

/**
 * @param {string} expectedMessage
 * @param {!HTMLElement} toast
 * @private
 */
function verifyErrorToastMessage(expectedMessage, toast) {
  assertTrue(toast.open);
  assertEquals(expectedMessage, toast.textContent.trim());
}

/**
 * Helper function that verifies that |printerList| matches the printers in
 * |entryList|.
 * @param {!HTMLElement} entryList
 * @param {!Array<!CupsPrinterInfo>} printerList
 * @private
 */
function verifyPrintersList(entryList, printerList) {
  for (let index = 0; index < printerList.length; ++index) {
    const entryInfo = entryList[index].printerEntry.printerInfo;
    const printerInfo = printerList[index];

    assertEquals(printerInfo.printerName, entryInfo.printerName);
    assertEquals(printerInfo.printerAddress, entryInfo.printerAddress);
    assertEquals(printerInfo.printerId, entryInfo.printerId);
  }
}

/**
 * Removes a saved printer located at |index| and then verify that saved
 * printers list is updated accordingly.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {!HTMLElement} savedPrinters
 * @param {!HTMLElement} entryList
 * @param {!Array<!CupsPrinterInfo>} printerList
 * @param {number} index
 * @return {!Promise}
 */
function removeAndVerifyPrinters(
    cupsPrintersBrowserProxy, savedPrinters, entryList, printerList, index) {
  clickThreeDotMenu(entryList[index]);
  savedPrinters.$$('#removeButton').click();

  return cupsPrintersBrowserProxy.whenCalled('removeCupsPrinter')
      .then(function() {
        // Simulate removing the printer from |cupsPrintersBrowserProxy|.
        printerList.splice(index, 1);

        Polymer.dom.flush();
        return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList');
      })
      .then(function() {
        verifyPrintersList(entryList, printerList);
      });
}

/**
 * @param {string} printerName
 * @param {string} printerAddress
 * @param {string} printerId
 * @return {!CupsPrinterInfo}
 * @private
 */
function createCupsPrinterInfo(printerName, printerAddress, printerId) {
  const printer = {
    ppdManufacturer: 'make',
    ppdModel: 'model',
    printerAddress: printerAddress,
    printerAutoconf: false,
    printerDescription: '',
    printerId: printerId,
    printerManufacturer: 'make',
    printerModel: 'model',
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
  };
  return printer;
}

suite('CupsSavedPrintersTests', function() {
  let page = null;
  let savedPrintersElement = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  /** @type {?Array<!CupsPrinterInfo>} */
  let printerList = null;

  setup(function() {
    printerList = [
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ];

    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;
    // |cupsPrinterBrowserProxy| needs to have a list of saved printers before
    // initializing the landing page.
    cupsPrintersBrowserProxy.printerList = {printerList: printerList};
    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    PolymerTest.clearBody();
    page = document.createElement('settings-cups-printers');
    // Enable feature flag to show the new saved printers list.
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.enableUpdatedUi_ = true;
    document.body.appendChild(page);
    assertTrue(!!page);

    Polymer.dom.flush();

    savedPrintersElement = page.$$('settings-cups-saved-printers');
    assertTrue(!!savedPrintersElement);
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    savedPrintersElement = null;
    printerList = null;
    page = null;
  });

  test('SavedPrintersSuccessfullyPopulates', function() {
    // List component contained by CupsSavedPrinters.
    const savedPrintersList =
        savedPrintersElement.$$('settings-cups-printers-entry-list');

    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(function() {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          const printerListEntries = getPrinterEntries(savedPrintersElement);

          verifyPrintersList(printerListEntries, printerList);

          assertEquals(printerList.length, printerListEntries.length);
        });
  });

  test('SuccessfullyRemoveMultipleSavedPrinters', function() {
    let savedPrinterEntries = [];

    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(function() {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          return removeAndVerifyPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement,
              savedPrinterEntries, printerList, 1 /** index */);
        })
        .then(function() {
          return removeAndVerifyPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement,
              savedPrinterEntries, printerList, 0 /** index */);
        })
        .then(function() {
          return removeAndVerifyPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement,
              savedPrinterEntries, printerList, 1 /** index */);
        })
        .then(function() {
          return removeAndVerifyPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement,
              savedPrinterEntries, printerList, 0 /** index */);
        });
  });

  test('UpdateSavedPrinter', function() {
    const expectedName = 'edited name';

    let editDialog = null;
    let savedPrinterEntries = null;

    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(function() {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          // Update the printer name of the first entry.
          clickThreeDotMenu(savedPrinterEntries[0]);
          savedPrintersElement.$$('#editButton').click();

          Polymer.dom.flush();

          editDialog = initializeEditDialog(page);

          // Change name of printer and save the change.
          const nameField = editDialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.fire('input');

          Polymer.dom.flush();

          clickSaveButton(editDialog);

          return cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
        })
        .then(function() {
          assertEquals(expectedName, editDialog.activePrinter.printerName);

          // Mimic changes to |cupsPrintersBrowserProxy.printerList|.
          printerList[0].printerName = expectedName;

          verifyPrintersList(savedPrinterEntries, printerList);
        });
  });

  test('ReconfigureSavedPrinter', function() {
    const expectedName = 'edited name';
    const expectedAddress = '1.1.1.1';

    let savedPrinterEntries = null;
    let editDialog = null;

    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(function() {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          // Edit the first entry.
          clickThreeDotMenu(savedPrinterEntries[0]);
          savedPrintersElement.$$('#editButton').click();

          Polymer.dom.flush();

          editDialog = initializeEditDialog(page);

          const nameField = editDialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.fire('input');

          const addressField = editDialog.$$('#printerAddress');
          assertTrue(!!addressField);
          addressField.value = expectedAddress;
          addressField.fire('input');

          Polymer.dom.flush();

          clickSaveButton(editDialog);

          return cupsPrintersBrowserProxy.whenCalled('reconfigureCupsPrinter');
        })
        .then(function() {
          assertEquals(expectedName, editDialog.activePrinter.printerName);
          assertEquals(
              expectedAddress, editDialog.activePrinter.printerAddress);

          // Mimic changes to |cupsPrintersBrowserProxy.printerList|.
          printerList[0].printerName = expectedName;
          printerList[0].printerAddress = expectedAddress;

          verifyPrintersList(savedPrinterEntries, printerList);
        });
  });
});

suite('CupsNearbyPrintersTests', function() {
  let page = null;
  let nearbyPrintersElement = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  setup(function() {
    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;

    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    PolymerTest.clearBody();
    page = document.createElement('settings-cups-printers');
    // Enable feature flag to show the new saved printers list.
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.enableUpdatedUi_ = true;
    document.body.appendChild(page);
    assertTrue(!!page);

    Polymer.dom.flush();

    nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
    assertTrue(!!nearbyPrintersElement);
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    nearbyPrintersElement = null;
    page = null;
  });

  test('nearbyPrintersSuccessfullyPopulates', function() {
    const automaticPrinterList = [
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ];
    const discoveredPrinterList = [
      createCupsPrinterInfo('test3', '3', 'id3'),
      createCupsPrinterInfo('test4', '4', 'id4'),
    ];

    return cupsPrintersBrowserProxy.whenCalled('startDiscoveringPrinters')
        .then(function() {
          // Assert that no printers have been detected.
          let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(0, nearbyPrinterEntries.length);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

          const expectedPrinterList =
              automaticPrinterList.concat(discoveredPrinterList);
          verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
          assertEquals(expectedPrinterList.length, nearbyPrinterEntries.length);
        });
  });

  test('addingAutomaticPrinterIsSuccessful', function() {
    const automaticPrinterList = [createCupsPrinterInfo('test1', '1', 'id1')];
    const discoveredPrinterList = [];

    return cupsPrintersBrowserProxy.whenCalled('startDiscoveringPrinters')
        .then(function() {
          // Assert that no printers are detected.
          let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(0, nearbyPrinterEntries.length);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          // Requery and assert that the newly detected printer automatic
          // printer has the correct icon button.
          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].$$('.icon-add-circle'));

          // Add an automatic printer and assert that that the toast
          // notification is shown.
          clickAddAutomaticButton(nearbyPrinterEntries[0]);

          Polymer.dom.flush();

          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(function() {
          const expectedToastMessage =
              'Added ' + automaticPrinterList[0].printerName;
          verifyErrorToastMessage(expectedToastMessage, page.$$('#errorToast'));
        });
  });

  test('addingDiscoveredPrinterIsSuccessful', function() {
    const automaticPrinterList = [];
    const discoveredPrinterList = [createCupsPrinterInfo('test3', '3', 'id3')];

    let manufacturerDialog = null;

    return cupsPrintersBrowserProxy.whenCalled('startDiscoveringPrinters')
        .then(function() {
          // Assert that there are initially no detected printers.
          let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(0, nearbyPrinterEntries.length);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          // Requery and assert that a newly detected discovered printer has the
          // correct icon button.
          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].$$('.action-button'));

          // Assert that clicking on the configure button shows the advanced
          // configuration dialog.
          clickConfigureButton(nearbyPrinterEntries[0]);

          Polymer.dom.flush();

          addDialog = page.$$('#addPrinterDialog');
          manufacturerDialog =
              addDialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!manufacturerDialog);

          return cupsPrintersBrowserProxy.whenCalled(
              'getCupsPrinterManufacturersList');
        })
        .then(function() {
          const addButton = manufacturerDialog.$$('#addPrinterButton');
          assertTrue(addButton.disabled);

          // Populate the manufacturer and model fields to enable the Add
          // button.
          manufacturerDialog.$$('#manufacturerDropdown').value = 'make';
          modelDropdown = manufacturerDialog.$$('#modelDropdown');
          modelDropdown.value = 'model';

          assertTrue(!addButton.disabled);

          addButton.click();
          return cupsPrintersBrowserProxy.whenCalled('addCupsPrinter');
        })
        .then(function() {
          // Assert that the toast notification is shown and has the expected
          // message when adding a discovered printer.
          const expectedToastMessage =
              'Added ' + discoveredPrinterList[0].printerName;
          verifyErrorToastMessage(expectedToastMessage, page.$$('#errorToast'));
        });
  });
});
