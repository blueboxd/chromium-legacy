// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './print_management_shared_css.js';
import './printing_manager.mojom-lite.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import './strings.js';


/**
 * Converts a mojo time to a JS time.
 * @param {!mojoBase.mojom.Time} mojoTime
 * @return {!Date}
 */
function convertMojoTimeToJS(mojoTime) {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
};

/**
 * Returns true if |date| is today, false otherwise.
 * @param {!Date} date
 * @return {boolean}
 */
function isToday(date) {
  const today_date = new Date();
  return date.getDate() === today_date.getDate() &&
         date.getMonth() === today_date.getMonth() &&
         date.getFullYear() === today_date.getFullYear();
};

/**
 * @fileoverview
 * 'print-job-entry' is contains a single print job entry and is used as a list
 * item.
 */
Polymer({
  is: 'print-job-entry',

  _template: html`{__html_template__}`,

  behaviors: [
    FocusRowBehavior,
  ],

  properties: {
    /** @type {!chromeos.printing.printingManager.mojom.PrintJobInfo} */
    jobEntry: Object,

    /** @private */
    jobTitle_: {
      type: String,
      computed: 'decodeString16_(jobEntry.title)',
    },

    /** @private */
    printerName_: {
      type: String,
      computed: 'decodeString16_(jobEntry.printerName)',
    },

    /** @private */
    creationTime_: {
      type: String,
      computed: 'computeDate_(jobEntry.creationTime)'
    },

    /** @private */
    completionStatus_: {
      type: String,
      computed: 'convertStatusToString_(jobEntry.completionStatus)',
    },
  },

  /**
   * Converts utf16 to a readable string.
   * @param {!mojoBase.mojom.String16} arr
   * @return {string}
   * @private
   */
  decodeString16_(arr) {
    return arr.data.map(ch => String.fromCodePoint(ch)).join('');
  },

  /**
   * Converts mojo time to JS time. Returns "Today" if |mojoTime| is at the
   * current day.
   * @param {!mojoBase.mojom.Time} mojoTime
   * @return {string}
   * @private
   */
  computeDate_(mojoTime) {
    const jsDate = convertMojoTimeToJS(mojoTime);
    // Date() is constructed with the current time in UTC. If the Date() matches
    // |jsDate|'s date, display the 12hour time of the current date.
    if (isToday(jsDate)) {
      return jsDate.toLocaleTimeString(/*locales=*/undefined,
        {hour12: true, hour: 'numeric', minute: 'numeric'});
    }
    // Remove the day of the week from the date.
    return jsDate.toLocaleDateString(/*locales=*/undefined,
        {month: 'short', day: 'numeric', year: 'numeric'});
  },

  /**
   * Returns the corresponding completion status from |mojoCompletionStatus|.
   * @param {number} mojoCompletionStatus
   * @return {string}
   * @private
   */
  convertStatusToString_(mojoCompletionStatus) {
    switch (mojoCompletionStatus) {
      case chromeos.printing.printingManager.mojom.PrintJobCompletionStatus
           .kFailed:
        return loadTimeData.getString('completionStatusFailed');
      case chromeos.printing.printingManager.mojom.PrintJobCompletionStatus
           .kCanceled:
        return loadTimeData.getString('completionStatusCanceled');
      case chromeos.printing.printingManager.mojom.PrintJobCompletionStatus
           .kPrinted:
        return loadTimeData.getString('completionStatusPrinted');
      default:
        assertNotReached();
        return loadTimeData.getString('completionStatusUnknownError');
    }
  },
});
