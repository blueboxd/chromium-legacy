// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Severity enum based on LogMessage format. Needs to stay in sync with the
 * MultideviceInternalsLogsHandler.
 * @enum {number}
 */
export const Severity = {
  VERBOSE: -1,
  INFO: 0,
  WARNING: 1,
  ERROR: 2,
};

/**
 * The type of log message object. The definition is based on
 * chrome/browser/ui/webui/chromeos/multidevice_internals/multidevice_internals_logs_handler.cc:
 * LogMessageToDictionary()
 * @typedef {{text: string,
 *            time: string,
 *            file: string,
 *            line: number,
 *            severity: Severity}}
 */
export let LogMessage;
