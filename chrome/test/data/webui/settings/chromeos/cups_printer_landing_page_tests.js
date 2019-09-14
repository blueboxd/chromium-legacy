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
  const addButton = printerEntry.$$('#savePrinterButton');
  assertTrue(!!addButton);
  addButton.click();
  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickSetupButton(printerEntry) {
  // Click on setup button on an item entry.
  const setupButton = printerEntry.$$('#setupPrinterButton');
  assertTrue(!!setupButton);
  setupButton.click();
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
  return entryList.querySelectorAll(
      'settings-cups-printers-entry:not([hidden])');
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
    assertEquals(entryList.length, printerList.length);
  }
}

/**
 * Removes a saved printer located at |index|.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {!HTMLElement} savedPrintersElement
 * @param {number} index
 * @return {!Promise}
 */
function removePrinter(cupsPrintersBrowserProxy, savedPrintersElement, index) {
  let printerList = cupsPrintersBrowserProxy.printerList.printerList;
  let savedPrinterEntries = getPrinterEntries(savedPrintersElement);

  clickThreeDotMenu(savedPrinterEntries[index]);
  savedPrintersElement.$$('#removeButton').click();

  return cupsPrintersBrowserProxy.whenCalled('removeCupsPrinter')
      .then(function() {
        // Simulate removing the printer from |cupsPrintersBrowserProxy|.
        printerList.splice(index, 1);

        // Simuluate saved printer changes.
        cr.webUIListenerCallback(
            'on-printers-changed', cupsPrintersBrowserProxy.printerList);
        Polymer.dom.flush();
      });
}

/**
 * Removes all saved printers through recursion.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {!HTMLElement} savedPrintersElement
 * @return {!Promise}
 */
function removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement) {
  let printerList = cupsPrintersBrowserProxy.printerList.printerList;
  let savedPrinterEntries = getPrinterEntries(savedPrintersElement);

  if (!printerList.length) {
    return Promise.resolve();
  }

  return removePrinter(
             cupsPrintersBrowserProxy, savedPrintersElement, 0 /* index */)
      .then(test_util.flushTasks)
      .then(removeAllPrinters.bind(
          this, cupsPrintersBrowserProxy, savedPrintersElement));
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

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    const mojom = chromeos.networkConfig.mojom;
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

    // Simulate internet connection.
    mojoApi_.resetForTest();
    mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
    const wifi1 =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi');
    wifi1.connectionState = mojom.ConnectionStateType.kConnected;
    mojoApi_.addNetworksForTest([wifi1]);

    PolymerTest.clearBody();
    settings.navigateTo(settings.routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    // Enable feature flag to show the new saved printers list.
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.enableUpdatedUi_ = true;
    document.body.appendChild(page);
    assertTrue(!!page);

    Polymer.dom.flush();
  });

  teardown(function() {
    mojoApi_.resetForTest();
    cupsPrintersBrowserProxy.reset();
    page.remove();
    savedPrintersElement = null;
    printerList = null;
    page = null;
  });

  test('SavedPrintersSuccessfullyPopulates', function() {
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          // List component contained by CupsSavedPrinters.
          const savedPrintersList =
              savedPrintersElement.$$('settings-cups-printers-entry-list');

          const printerListEntries = getPrinterEntries(savedPrintersElement);

          verifyPrintersList(printerListEntries, printerList);
        });
  });

  test('SuccessfullyRemoveMultipleSavedPrinters', function() {
    let savedPrinterEntries = [];

    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          return removeAllPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement);
        })
        .then(() => {
          let entryList = getPrinterEntries(savedPrintersElement);
          verifyPrintersList(entryList, printerList);
        });
  });

  test('HideSavedPrintersWhenEmpty', function() {
    // List component contained by CupsSavedPrinters.
    let savedPrintersList = [];
    let savedPrinterEntries = [];

    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrintersList =
              savedPrintersElement.$$('settings-cups-printers-entry-list');
          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          verifyPrintersList(savedPrinterEntries, printerList);

          assertTrue(!!page.$$('#savedPrinters'));

          return removeAllPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement);
        })
        .then(() => {
          assertFalse(!!page.$$('#savedPrinters'));
        });
  });

  test('UpdateSavedPrinter', function() {
    const expectedName = 'edited name';

    let editDialog = null;
    let savedPrinterEntries = null;

    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

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
        .then(() => {
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
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

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
        .then(() => {
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

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    const mojom = chromeos.networkConfig.mojom;
    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;

    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    // Simulate internet connection.
    mojoApi_.resetForTest();
    const wifi1 =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1');
    wifi1.connectionState = mojom.ConnectionStateType.kOnline;
    mojoApi_.addNetworksForTest([wifi1]);

    PolymerTest.clearBody();
    settings.navigateTo(settings.routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    // Enable feature flag to show the new saved printers list.
    // TODO(jimmyxgong): Remove this line when the feature flag is removed.
    page.enableUpdatedUi_ = true;
    document.body.appendChild(page);
    assertTrue(!!page);

    Polymer.dom.flush();
  });

  teardown(function() {
    mojoApi_.resetForTest();
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

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

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
    });
  });

  test('nearbyPrintersSortOrderAutoFirstThenDiscovered', function() {
    let discoveredPrinterA =
        createCupsPrinterInfo('printerNameA', 'printerAddress1', 'printerId1');
    let discoveredPrinterB =
        createCupsPrinterInfo('printerNameB', 'printerAddress2', 'printerId2');
    let discoveredPrinterC =
        createCupsPrinterInfo('printerNameC', 'printerAddress3', 'printerId3');
    let autoPrinterD =
        createCupsPrinterInfo('printerNameD', 'printerAddress4', 'printerId4');
    let autoPrinterE =
        createCupsPrinterInfo('printerNameE', 'printerAddress5', 'printerId5');
    let autoPrinterF =
        createCupsPrinterInfo('printerNameF', 'printerAddress6', 'printerId6');

    // Add printers in a non-alphabetical order to test sorting.
    const automaticPrinterList = [autoPrinterF, autoPrinterD, autoPrinterE];
    const discoveredPrinterList =
        [discoveredPrinterC, discoveredPrinterA, discoveredPrinterB];

    // Expected sort order is to sort automatic printers first then
    // sort discovered printers
    const expectedPrinterList = [
      autoPrinterD, autoPrinterE, autoPrinterF, discoveredPrinterA,
      discoveredPrinterB, discoveredPrinterC
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      // Assert that no printers have been detected.
      let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
      assertEquals(0, nearbyPrinterEntries.length);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', automaticPrinterList,
          discoveredPrinterList);

      Polymer.dom.flush();

      nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

      verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
    });
  });

  test('addingAutomaticPrinterIsSuccessful', function() {
    const automaticPrinterList = [createCupsPrinterInfo('test1', '1', 'id1')];
    const discoveredPrinterList = [];

    return test_util.flushTasks()
        .then(() => {
          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          // Assert that no printers are detected.
          let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(0, nearbyPrinterEntries.length);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          // Requery and assert that the newly detected printer automatic
          // printer has the correct button.
          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].$$('#savePrinterButton'));

          // Add an automatic printer and assert that that the toast
          // notification is shown.
          clickAddAutomaticButton(nearbyPrinterEntries[0]);

          Polymer.dom.flush();

          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(() => {
          const expectedToastMessage =
              'Added ' + automaticPrinterList[0].printerName;
          verifyErrorToastMessage(expectedToastMessage, page.$$('#errorToast'));
        });
  });

  test('addingDiscoveredPrinterIsSuccessful', function() {
    const automaticPrinterList = [];
    const discoveredPrinterList = [createCupsPrinterInfo('test3', '3', 'id3')];

    let manufacturerDialog = null;

    return test_util.flushTasks()
        .then(() => {
          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          // Assert that there are initially no detected printers.
          let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(0, nearbyPrinterEntries.length);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          // Requery and assert that a newly detected discovered printer has
          // the correct icon button.
          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].$$('#setupPrinterButton'));

          // Assert that clicking on the setup button shows the advanced
          // configuration dialog.
          clickSetupButton(nearbyPrinterEntries[0]);

          Polymer.dom.flush();

          addDialog = page.$$('#addPrinterDialog');
          manufacturerDialog =
              addDialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!manufacturerDialog);

          return cupsPrintersBrowserProxy.whenCalled(
              'getCupsPrinterManufacturersList');
        })
        .then(() => {
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
        .then(() => {
          // Assert that the toast notification is shown and has the expected
          // message when adding a discovered printer.
          const expectedToastMessage =
              'Added ' + discoveredPrinterList[0].printerName;
          verifyErrorToastMessage(expectedToastMessage, page.$$('#errorToast'));
        });
  });

  test('NetworkConnectedButNoInternet', function() {
    // Simulate connecting to a network with no internet connection.
    mojoApi_.setNetworkConnectionStateForTest(
        'wifi1_guid',
        chromeos.networkConfig.mojom.ConnectionStateType.kConnected);
    return test_util.flushTasks().then(() => {
      // We require internet to be able to add a new printer. Connecting to
      // a network without connectivity should be equivalent to not being
      // connected to a network.
      assertTrue(!!page.$$('#cloudOffIcon'));
      assertTrue(!!page.$$('#connectionMessage'));
      assertTrue(!!page.$$('#addManualPrinterIcon').disabled);
    });
  });

  test('checkNetworkConnection', function() {
    // Simulate disconnecting from a network.
    mojoApi_.setNetworkConnectionStateForTest(
        'wifi1_guid',
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected);
    return test_util.flushTasks()
        .then(() => {
          // Expect offline text to show up when no internet is
          // connected.
          assertTrue(!!page.$$('#cloudOffIcon'));
          assertTrue(!!page.$$('#connectionMessage'));
          assertTrue(!!page.$$('#addManualPrinterIcon').disabled);

          // Simulate connecting to a network with connectivity.
          mojoApi_.setNetworkConnectionStateForTest(
              'wifi1_guid',
              chromeos.networkConfig.mojom.ConnectionStateType.kOnline);
          return test_util.flushTasks();
        })
        .then(() => {
          const automaticPrinterList = [
            createCupsPrinterInfo('test1', '1', 'id1'),
            createCupsPrinterInfo('test2', '2', 'id2'),
          ];
          const discoveredPrinterList = [
            createCupsPrinterInfo('test3', '3', 'id3'),
            createCupsPrinterInfo('test4', '4', 'id4'),
          ];

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

          const expectedPrinterList =
              automaticPrinterList.concat(discoveredPrinterList);
          verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
        });
  });

  test('No Nearby Printers', function() {
    const automaticPrinterList = [];
    const discoveredPrinterList = [createCupsPrinterInfo('test3', '3', 'id3')];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      // Assert that there are no nearby printers.
      let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
      assertEquals(0, nearbyPrinterEntries.length);

      // Check that the "No available printers" message is shown.
      assertFalse(nearbyPrintersElement.$$('#noPrinterMessage').hidden);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', automaticPrinterList,
          discoveredPrinterList);

      Polymer.dom.flush();

      // Check that the "No available printers" message is not shown.
      assertTrue(nearbyPrintersElement.$$('#noPrinterMessage').hidden);
    });
  });
});
