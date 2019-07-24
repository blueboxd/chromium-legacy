// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * @typedef {{selectSaveAsPdfDestination: boolean,
   *            layoutSettings.portrait: boolean,
   *            pageRange: string,
   *            headersAndFooters: boolean,
   *            backgroundColorsAndImages: boolean,
   *            margins: number}}
   * @see chrome/browser/printing/print_preview_pdf_generated_browsertest.cc
   */
  let PreviewSettings;

  /**
   * @typedef {{
   *   deviceName: string,
   *   printerName: string,
   *   printerDescription: (string | undefined),
   *   cupsEnterprisePrinter: (boolean | undefined),
   *   printerOptions: (Object | undefined),
   *   policies: (print_preview.Policies | undefined),
   * }}
   */
  let LocalDestinationInfo;

  /**
   * @typedef {{
   *   isInKioskAutoPrintMode: boolean,
   *   isInAppKioskMode: boolean,
   *   uiLocale: string,
   *   thousandsDelimeter: string,
   *   decimalDelimeter: string,
   *   unitType: !print_preview.MeasurementSystemUnitType,
   *   previewModifiable: boolean,
   *   documentTitle: string,
   *   documentHasSelection: boolean,
   *   shouldPrintSelectionOnly: boolean,
   *   printerName: string,
   *   headerFooter: (boolean | undefined),
   *   isHeaderFooterManaged: boolean,
   *   serializedAppStateStr: ?string,
   *   serializedDefaultDestinationSelectionRulesStr: ?string,
   *   cloudPrintURL: (string | undefined),
   *   userAccounts: (Array<string> | undefined),
   *   syncAvailable: boolean
   * }}
   * @see corresponding field name definitions in print_preview_handler.cc
   */
  let NativeInitialSettings;

  /**
   * @typedef {{
   *   serviceName: string,
   *   name: string,
   *   hasLocalPrinting: boolean,
   *   isUnregistered: boolean,
   *   cloudID: string,
   * }}
   * @see PrintPreviewHandler::FillPrinterDescription in
   * print_preview_handler.cc
   */
  let PrivetPrinterDescription;

  /**
   * @typedef {{
   *   printer:(print_preview.PrivetPrinterDescription |
   *            print_preview.LocalDestinationInfo |
   *            undefined),
   *   capabilities: !print_preview.Cdd,
   * }}
   */
  let CapabilitiesResponse;

  /**
   * @typedef {{
   *   printerId: string,
   *   success: boolean,
   *   capabilities: !print_preview.Cdd,
   *   policies: (print_preview.Policies | undefined),
   * }}
   */
  let PrinterSetupResponse;

  /**
   * @typedef {{
   *   extensionId: string,
   *   extensionName: string,
   *   id: string,
   *   name: string,
   *   description: (string|undefined),
   * }}
   */
  let ProvisionalDestinationInfo;

  /**
   * Printer types for capabilities and printer list requests.
   * Should match PrinterType in print_preview_handler.h
   * @enum {number}
   */
  const PrinterType = {
    PRIVET_PRINTER: 0,
    EXTENSION_PRINTER: 1,
    PDF_PRINTER: 2,
    LOCAL_PRINTER: 3,
    CLOUD_PRINTER: 4
  };

  /**
   * An interface to the native Chromium printing system layer.
   */
  class NativeLayer {
    /**
     * Creates a new NativeLayer if the current instance is not set.
     * @return {!print_preview.NativeLayer} The singleton instance.
     */
    static getInstance() {
      if (currentInstance == null) {
        currentInstance = new NativeLayer();
      }
      return assert(currentInstance);
    }

    /**
     * @param {!print_preview.NativeLayer} instance The NativeLayer instance
     *     to set for print preview construction.
     */
    static setInstance(instance) {
      currentInstance = instance;
    }

    // <if expr="chromeos">
    /**
     * Requests access token for cloud print requests for DEVICE origin.
     * @return {!Promise<string>}
     */
    getAccessToken() {
      return cr.sendWithPromise('getAccessToken');
    }
    // </if>

    /**
     * Gets the initial settings to initialize the print preview with.
     * @return {!Promise<!print_preview.NativeInitialSettings>}
     */
    getInitialSettings() {
      return cr.sendWithPromise('getInitialSettings');
    }

    /**
     * Requests the system's print destinations. The promise will be resolved
     * when all destinations of that type have been retrieved. One or more
     * 'printers-added' events may be fired in response before resolution.
     * @param {!print_preview.PrinterType} type The type of destinations to
     *     request.
     * @return {!Promise}
     */
    getPrinters(type) {
      return cr.sendWithPromise('getPrinters', type);
    }

    /**
     * Requests the destination's printing capabilities. Returns a promise that
     * will be resolved with the capabilities if they are obtained successfully.
     * @param {string} destinationId ID of the destination.
     * @param {!print_preview.PrinterType} type The destination's printer type.
     * @return {!Promise<!print_preview.CapabilitiesResponse>}
     */
    getPrinterCapabilities(destinationId, type) {
      return cr.sendWithPromise(
          'getPrinterCapabilities', destinationId,
          destinationId ==
                  print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ?
              print_preview.PrinterType.PDF_PRINTER :
              type);
    }

    // <if expr="chromeos">
    /**
     * Requests Chrome to resolve provisional extension destination by granting
     * the provider extension access to the printer.
     * @param {string} provisionalDestinationId
     * @return {!Promise<!print_preview.ProvisionalDestinationInfo>}
     */
    grantExtensionPrinterAccess(provisionalDestinationId) {
      return cr.sendWithPromise('grantExtensionPrinterAccess',
                                provisionalDestinationId);
    }

    /**
     * Requests that Chrome peform printer setup for the given printer.
     * @param {string} printerId
     * @return {!Promise<!print_preview.PrinterSetupResponse>}
     */
    setupPrinter(printerId) {
      return cr.sendWithPromise('setupPrinter', printerId);
    }
    // </if>

    /**
     * Requests that a preview be generated. The following Web UI events may
     * be triggered in response:
     *   'print-preset-options',
     *   'page-count-ready',
     *   'page-layout-ready',
     *   'page-preview-ready'
     * @param {string} printTicket JSON print ticket for the request.
     * @return {!Promise<number>} Promise that resolves with the unique ID of
     *     the preview UI when the preview has been generated.
     */
    getPreview(printTicket) {
      return cr.sendWithPromise('getPreview', printTicket);
    }

    /**
     * Opens the chrome://settings printing page. For Chrome OS, open the
     *  printing settings in the Settings App.
     */
    openSettingsPrintPage() {
      // <if expr="chromeos">
      chrome.send('openPrinterSettings');
      // </if>
      // <if expr="not chromeos">
      window.open('chrome://settings/printing');
      // </if>
    }

    /**
     * Requests that the document be printed.
     * @param {string} printTicket The serialized print ticket for the print
     *     job.
     * @return {!Promise} Promise that will resolve when the print request is
     *     finished or rejected.
     */
    print(printTicket) {
      return cr.sendWithPromise('print', printTicket);
    }

    /** Requests that the current pending print request be cancelled. */
    cancelPendingPrintRequest() {
      chrome.send('cancelPendingPrintRequest');
    }

    /**
     * Sends the app state to be saved in the sticky settings.
     * @param {string} appStateStr JSON string of the app state to persist.
     */
    saveAppState(appStateStr) {
      chrome.send('saveAppState', [appStateStr]);
    }

    // <if expr="not chromeos and not is_win">
    /** Shows the system's native printing dialog. */
    showSystemDialog() {
      chrome.send('showSystemDialog');
    }
    // </if>

    /**
     * Closes the print preview dialog.
     * If |isCancel| is true, also sends a message to Print Preview Handler in
     * order to update UMA statistics.
     * @param {boolean} isCancel whether this was called due to the user
     *     closing the dialog without printing.
     */
    dialogClose(isCancel) {
      if (isCancel) {
        chrome.send('closePrintPreviewDialog');
      }
      chrome.send('dialogClose');
    }

    /** Hide the print preview dialog and allow the native layer to close it. */
    hidePreview() {
      chrome.send('hidePreview');
    }

    /**
     * Opens the Google Cloud Print sign-in tab. If the user signs in
     * successfully, the user-accounts-updated event will be sent in response.
     * @param {boolean} addAccount Whether to open an 'add a new account' or
     *     default sign in page.
     */
    signIn(addAccount) {
      chrome.send('signIn', [addAccount]);
    }

    /**
     * Sends a message to the test, letting it know that an
     * option has been set to a particular value and that the change has
     * finished modifying the preview area.
     */
    uiLoadedForTest() {
      chrome.send('UILoadedForTest');
    }

    /**
     * Notifies the test that the option it tried to change
     * had not been changed successfully.
     */
    uiFailedLoadingForTest() {
      chrome.send('UIFailedLoadingForTest');
    }

    /**
     * Notifies the metrics handler to record a histogram value.
     * @param {string} histogram The name of the histogram to record
     * @param {number} bucket The bucket to record
     * @param {number} maxBucket The maximum bucket value in the histogram.
     */
    recordInHistogram(histogram, bucket, maxBucket) {
      chrome.send(
          'metricsHandler:recordInHistogram', [histogram, bucket, maxBucket]);
    }
  }

  /** @private {?print_preview.NativeLayer} */
  let currentInstance = null;

  // Export
  return {
    CapabilitiesResponse: CapabilitiesResponse,
    LocalDestinationInfo: LocalDestinationInfo,
    NativeInitialSettings: NativeInitialSettings,
    NativeLayer: NativeLayer,
    PreviewSettings: PreviewSettings,
    PrinterSetupResponse: PrinterSetupResponse,
    PrinterType: PrinterType,
    PrivetPrinterDescription: PrivetPrinterDescription,
    ProvisionalDestinationInfo: ProvisionalDestinationInfo,
  };
});
