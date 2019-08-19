/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Checks the hasEnrolledInstrument() value for 'basic-card' with the given
 * options.
 * @param {PaymentOptions} options - The payment options to use.
 * @return {Promise<boolean|string>} The boolean value of
 * hasEnrolledInstrument() or the error message string.
 */
async function hasEnrolledInstrument(options) { // eslint-disable-line no-unused-vars,max-len
  try {
    const result =
        await new PaymentRequest(
            [{supportedMethods: 'basic-card'}], {
              total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
            },
            options)
            .hasEnrolledInstrument();
    return result;
  } catch (e) {
    return e.toString();
  }
}
