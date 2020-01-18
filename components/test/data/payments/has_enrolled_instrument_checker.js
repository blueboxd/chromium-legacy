/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Checks whether the given payment method has an enrolled instrument.
 * @param {string} method - The payment method identifier to check.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function hasEnrolledInstrument(method) { // eslint-disable-line no-unused-vars, max-len
  try {
    const request = new PaymentRequest(
        [{supportedMethods: method}],
        {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});
    const result = await request.hasEnrolledInstrument();
    return result ? 'true' : 'false';
  } catch (e) {
    return e.message;
  }
}
