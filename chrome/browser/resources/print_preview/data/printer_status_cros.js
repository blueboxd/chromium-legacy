// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 *  These values must be kept in sync with the Reason enum in
 *  /chromeos/printing/cups_printer_status.h
 *  @enum {number}
 */
export const PrinterStatusReason = {
  CONNECTING_TO_DEVICE: 0,
  DEVICE_ERROR: 1,
  DOOR_OPEN: 2,
  LOW_ON_INK: 3,
  LOW_ON_PAPER: 4,
  NO_ERROR: 5,
  OUT_OF_INK: 6,
  OUT_OF_PAPER: 7,
  OUTPUT_ALMOST_FULL: 8,
  OUTPUT_FULL: 9,
  PAPER_JAM: 10,
  PAUSED: 11,
  PRINTER_QUEUE_FULL: 12,
  PRINTER_UNREACHABLE: 13,
  STOPPED: 14,
  TRAY_MISSING: 15,
  UNKNOWN_REASON: 16,
};

/**
 *  These values must be kept in sync with the Severity enum in
 *  /chromeos/printing/cups_printer_status.h
 *  @enum {number}
 */
export const PrinterStatusSeverity = {
  UNKNOWN_SEVERITY: 0,
  REPORT: 1,
  WARNING: 2,
  ERROR: 3,
};

/**
 * A container for the results of a printer status query. A printer status query
 * can return multiple error reasons. |timestamp| is set at the time of status
 * creation.
 *
 * @typedef {{
 *   printerId: string,
 *   statusReasons: !Array<{
 *     reason: PrinterStatusReason,
 *     severity: PrinterStatusSeverity,
 *   }>,
 *   timestamp: number,
 * }}
 */
export let PrinterStatus;

/** @const {!Map<!PrinterStatusReason, string>} */
export const ERROR_STRING_KEY_MAP = new Map([
  [PrinterStatusReason.CONNECTING_TO_DEVICE, 'printerStatusConnectingToDevice'],
  [PrinterStatusReason.DEVICE_ERROR, 'printerStatusDeviceError'],
  [PrinterStatusReason.DOOR_OPEN, 'printerStatusDoorOpen'],
  [PrinterStatusReason.LOW_ON_INK, 'printerStatusLowOnInk'],
  [PrinterStatusReason.LOW_ON_PAPER, 'printerStatusLowOnPaper'],
  [PrinterStatusReason.OUT_OF_INK, 'printerStatusOutOfInk'],
  [PrinterStatusReason.OUT_OF_PAPER, 'printerStatusOutOfPaper'],
  [PrinterStatusReason.OUTPUT_ALMOST_FULL, 'printerStatusOutputAlmostFull'],
  [PrinterStatusReason.OUTPUT_FULL, 'printerStatusOutputFull'],
  [PrinterStatusReason.PAPER_JAM, 'printerStatusPaperJam'],
  [PrinterStatusReason.PAUSED, 'printerStatusPaused'],
  [PrinterStatusReason.PRINTER_QUEUE_FULL, 'printerStatusPrinterQueueFull'],
  [PrinterStatusReason.PRINTER_UNREACHABLE, 'printerStatusPrinterUnreachable'],
  [PrinterStatusReason.STOPPED, 'printerStatusStopped'],
  [PrinterStatusReason.TRAY_MISSING, 'printerStatusTrayMissing'],
]);
