// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Types of error used in ERROR metrics.
 * @enum {string}
 */
export const ErrorType = {
  UNCAUGHT_PROMISE: 'uncaught-promise',
};

/**
 * Error level used in ERROR metrics.
 * @enum {string}
 */
export const ErrorLevel = {
  WARNING: 'WARNING',
  ERROR: 'ERROR',
};

/**
 * Code location of stack frame.
 * @typedef {{
 *   fileName: string,
 *   funcName: string,
 *   lineNo: number,
 *   colNo : number,
 *  }}
 */
export let StackFrame;

/**
 * Converts v8 CallSite object to StackFrame.
 * @param {!CallSite} callsite
 * @return {!StackFrame}
 */
function toStackFrame(callsite) {
  // TODO(crbug/1072700): Handle native frame.
  let fileName = callsite.getFileName() || 'unknown';
  if (fileName.startsWith(window.location.origin)) {
    fileName = fileName.substring(window.location.origin.length + 1);
  }
  const ensureNumber = (n) => (n === undefined ? -1 : n);
  return {
    fileName,
    funcName: callsite.getFunctionName() || '[Anonymous]',
    lineNo: ensureNumber(callsite.getLineNumber()),
    colNo: ensureNumber(callsite.getColumnNumber()),
  };
}

/**
 * Gets stack frames from error.
 * @param {!Error} error
 * @return {?Array<!StackFrame>} return null if failed to get frames from error.
 */
export function getStackFrames(error) {
  const prevPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = (error, stack) => {
    try {
      return stack.map(toStackFrame);
    } catch (e) {
      console.error('Failed to prepareStackTrace', e);
      return null;
    }
  };

  const /** (?Array<!StackFrame>|string) */ frames = error.stack;
  Error.prepareStackTrace = prevPrepareStackTrace;

  if (typeof frames !== 'object') {
    return null;
  }
  return /** @type {?Array<!StackFrame>} */ (frames);
}
