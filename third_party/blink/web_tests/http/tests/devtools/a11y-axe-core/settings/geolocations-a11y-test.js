// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      'Tests accessibility in the settings tool geolocations pane using the axe-core linter.');

  await TestRunner.loadModule('axe_core_test_runner');
  await UI.viewManager.showView('emulation-geolocations');
  const geolocationsWidget = await UI.viewManager.view('emulation-geolocations').widget();

  async function testAddLocation() {
    const addLocationButton = geolocationsWidget._defaultFocusedElement;
    addLocationButton.click();

    const newLocationInputs = geolocationsWidget._list._editor._controls;
    TestRunner.addResult(`Opened input box: ${!!newLocationInputs}`);

    await AxeCoreTestRunner.runValidation(geolocationsWidget.contentElement);
  }

  async function testNewLocationError() {
    const locationsEditor = geolocationsWidget._list._editor;
    const newLocationInputs = locationsEditor._controls;
    const nameInput = newLocationInputs[0];
    TestRunner.addResult(`Invalidating the ${nameInput.getAttribute('aria-label')} input`);
    nameInput.blur();

    const errorMessage = locationsEditor._errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    await AxeCoreTestRunner.runValidation(geolocationsWidget.contentElement);
  }

  TestRunner.runAsyncTestSuite([testAddLocation, testNewLocationError]);
})();