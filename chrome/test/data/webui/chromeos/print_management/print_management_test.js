// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): use es6 module for mojo binding crbug/1004256
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://print-management/print_management.js';

import {setMetadataProviderForTesting} from 'chrome://print-management/mojo_interface_provider.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const CompletionStatus = {
  FAILED: 0,
  CANCELED: 1,
  PRINTED: 2,
};

/**
 * Converts a JS string to mojo_base::mojom::String16 object.
 * @param {string} str
 * @return {!object}
 */
function strToMojoString16(str) {
  let arr = [];
  for (var i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }
  return {data: arr};
}

/**
 * Converts a JS time (milliseconds since UNIX epoch) to mojom::time
 * (microseconds since WINDOWS epoch).
 * @param {Date} jsDate
 * @return {number}
 */
function convertToMojoTime(jsDate) {
  const windowsEpoch = new Date(Date.UTC(1601, 0, 1, 0, 0, 0));
  const jsEpoch = new Date(Date.UTC(1970, 0, 1, 0, 0, 0));
  return ((jsEpoch - windowsEpoch) * 1000) + (jsDate.getTime() * 1000);
}

/**
 * @param{string} id
 * @param{string} title
 * @param{number} completionStatus
 * @param{Date}   jsDate
 * @param{string} printerName
 * @return {!Object}
 */
function createJobEntry(id, title, completionStatus, jsDate, printerName) {
  let jobEntry = {};
  jobEntry.id = id;
  jobEntry.title = strToMojoString16(title);
  jobEntry.completionStatus = completionStatus;
  jobEntry.creationTime = {internalValue: convertToMojoTime(jsDate)};
  jobEntry.printerName = strToMojoString16(printerName);
  jobEntry.printerUri = {url: '192.168.1.1'};
  jobEntry.numberOfPages = 1;

  return jobEntry;
}

/**
 * @param{!Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
 *     expected
 * @param{!Array<!HTMLElement>} actual
 */
function verifyPrintJobs(expected, actual) {
  assertEquals(expected.length, actual.length);
  for (let i = 0; i < expected.length; i++) {
    const actualJobInfo = actual[i].jobEntry;
    assertEquals(expected[i].id, actualJobInfo.id);
    assertEquals(expected[i].title, actualJobInfo.title);
    assertEquals(expected[i].completionStatus, actualJobInfo.completionStatus);
    assertEquals(expected[i].creationTime, actualJobInfo.creationTime);
    assertEquals(expected[i].printerName, actualJobInfo.printerName);
    assertEquals(expected[i].creationTime, actualJobInfo.creationTime);
  }
}

/**
 * @param {!HTMLElement} page
 * @return {!Array<!HTMLElement>}
 */
function getPrintJobEntries(page) {
  const entryList = page.$$('#entryList');
  return Array.from(
      entryList.querySelectorAll('print-job-entry:not([hidden])'));
}

class FakePrintingMetadataProvider {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /** @type {!Array<chromeos.printing.printingManager.mojom.PrintJobInfo>} */
    this.printJobs_ = [];

    this.resetForTest();
  }

  resetForTest() {
    this.printJobs_ = [];
    this.resolverMap_.set('getPrintJobs', new PromiseResolver());
    this.resolverMap_.set('deleteAllPrintJobs', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    let method = this.resolverMap_.get(methodName);
    assert(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  /**
   * @param {string} methodName
   * @protected
   */
  methodCalled(methodName) {
    this.getResolver_(methodName).resolve();
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /**
   * @param {?Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
   *     printJobs
   */
  setPrintJobs(printJobs) {
    this.printJobs_ = printJobs;
  }

  /**
   * @param {chromeos.printing.printingManager.mojom.PrintJobInfo} job
   */
  addPrintJob(job) {
    this.printJobs_ = this.printJobs_.concat(job);
  }

  // printingMetadataProvider methods

  /**
   * @return {!Promise<{printJobs:
   *     !Array<chromeos.printing.printingManager.mojom.PrintJobInfo>}>}
   */
  getPrintJobs() {
    return new Promise(resolve => {
      this.methodCalled('getPrintJobs');
      resolve({printJobs: this.printJobs_ || []});
    });
  }

  /** @return {!Promise<{success: boolean}>} */
  deleteAllPrintJobs() {
    return new Promise(resolve => {
      this.printJobs_ = [];
      this.methodCalled('deleteAllPrintJobs');
      resolve({success: true});
    });
  }
}

suite('PrintManagementTest', () => {
  /** @type {?PrintManagementElement} */
  let page = null;

  /**
   * @type {
   *    ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderRemote
   *  }
   */
  let mojoApi_;

  suiteSetup(() => {
    mojoApi_ = new FakePrintingMetadataProvider();
    setMetadataProviderForTesting(mojoApi_);
  });

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    mojoApi_.resetForTest();
    page.remove();
    page = null;
  });

  /**
   * @param {?Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
   *     printJobs
   */
  function initializePrintManagementApp(printJobs) {
    mojoApi_.setPrintJobs(printJobs);
    page = document.createElement('print-management');
    document.body.appendChild(page);
    assert(!!page);
    flush();
  }

  test('PrintHistoryListIsSortedReverseChronologically', () => {
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA', CompletionStatus.PRINTED,
          new Date(Date.UTC(2020, 3, 1, 1, 1, 1)), 'nameA'),
      createJobEntry(
          'middle', 'titleA', CompletionStatus.PRINTED,
          new Date(Date.UTC(2020, 2, 1, 1, 1, 1)), 'nameB'),
      createJobEntry(
          'oldest', 'titleC', CompletionStatus.PRINTED,
          new Date(Date.UTC(2020, 1, 1, 1, 1, 1)), 'nameC')
    ];

    // Initialize with a reversed array of |expectedArr|, since we expect the
    // app to sort the list when it first loads. Since reverse() mutates the
    // original array, use a copy array to prevent mutating |expectedArr|.
    initializePrintManagementApp(expectedArr.slice().reverse());
    return mojoApi_.whenCalled('getPrintJobs').then(() => {
      flush();
      verifyPrintJobs(expectedArr, getPrintJobEntries(page));
    });
  });

  test('ClearAllButtonDisabledWhenNoPrintJobsSaved', () => {
    // Initialize with no saved print jobs, expect the clear all button to be
    // disabled.
    initializePrintManagementApp(/*printJobs=*/[]);
    return mojoApi_.whenCalled('getPrintJobs').then(() => {
      flush();
      assertTrue(page.$$('#clearAllButton').disabled);
    });
  });

  test('ClearAllPrintHistory', () => {
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA', CompletionStatus.PRINTED,
          new Date(Date.UTC('February 5, 2020 03:24:00')), 'nameA'),
      createJobEntry(
          'fileB', 'titleB', CompletionStatus.PRINTED,
          new Date(Date.UTC('February 6, 2020 03:24:00')), 'nameB'),
      createJobEntry(
          'fileC', 'titleC', CompletionStatus.PRINTED,
          new Date(Date.UTC('February 7, 2020 03:24:00')), 'nameC'),
    ];

    initializePrintManagementApp(expectedArr);
    return mojoApi_.whenCalled('getPrintJobs')
        .then(() => {
          flush();
          verifyPrintJobs(expectedArr, getPrintJobEntries(page));

          // Click the clear all button.
          const button = page.$$('#clearAllButton');
          button.click();
          flush();
          // Verify that the confirmation dialog shows up and click on the
          // confirmation button.
          const dialog = page.$$('#clearHistoryDialog');
          assertTrue(!!dialog);
          assertTrue(!dialog.$$('.action-button').disabled);
          dialog.$$('.action-button').click();
          assertTrue(dialog.$$('.action-button').disabled);
          return mojoApi_.whenCalled('deleteAllPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs(/*expected=*/[], getPrintJobEntries(page));
          assertTrue(page.$$('#clearAllButton').disabled);
        });
  });
});

suite('PrintJobEntryTest', () => {
  /** @type {?HTMLElement} */
  let jobEntryTestElement = null;

  /**
   * @type {
   *    ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderRemote
   *  }
   */
  let mojoApi_;

  suiteSetup(() => {
    mojoApi_ = new FakePrintingMetadataProvider();
    setMetadataProviderForTesting(mojoApi_);
  });

  setup(() => {
    jobEntryTestElement = document.createElement('print-job-entry');
    assertTrue(!!jobEntryTestElement);
    document.body.appendChild(jobEntryTestElement);
  });

  teardown(() => {
    jobEntryTestElement.remove();
    jobEntryTestElement = null;
  });

  /**
   * @param {!HTMLElement} element
   * @param {number} newStatus
   * @param {string} expectedStatus
   */
  function updateAndVerifyCompletionStatus(element, newStatus, expectedStatus) {
    element.set('jobEntry.completionStatus', newStatus);
    assertEquals(
        expectedStatus, element.$$('#completionStatus').textContent.trim());
  }

  test('initializeJobEntry', () => {
    const expectedTitle = 'title';
    const expectedStatus = CompletionStatus.PRINTED;
    const expectedPrinterName = 'printer name';
    const expectedCreationTime = new Date();

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle, expectedStatus, expectedCreationTime,
        expectedPrinterName);

    // Assert the title, printer name, creation time, and status are displayed
    // correctly.
    assertEquals(
        expectedTitle, jobEntryTestElement.$$('#jobTitle').textContent.trim());
    assertEquals(
        expectedPrinterName,
        jobEntryTestElement.$$('#printerName').textContent.trim());
    assertEquals(
        'Printed',
        jobEntryTestElement.$$('#completionStatus').textContent.trim());

    // Change date and assert it shows the correct date (Feb 5, 2020);
    jobEntryTestElement.set('jobEntry.creationTime', {
      internalValue: convertToMojoTime(new Date('February 5, 2020 03:24:00'))
    });
    assertEquals(
        'Feb 5, 2020',
        jobEntryTestElement.$$('#creationTime').textContent.trim());

    // Change the completion status and verify it shows the correct status.
    updateAndVerifyCompletionStatus(
        jobEntryTestElement, CompletionStatus.FAILED, 'Failed');
    updateAndVerifyCompletionStatus(
        jobEntryTestElement, CompletionStatus.CANCELED, 'Canceled');
  });
});