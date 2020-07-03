// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Runs the given test in the untrusted context.
 * @param {string} testName
 * @return {boolean|error}
 */
async function runTestInUntrusted(testName) {
  try {
    await untrustedMessagePipe.sendMessage('run-test-case', testName);
    return true;
  } catch (err) {
    return err;
  }
}
