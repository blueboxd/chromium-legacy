/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Returns the status field from the payment handler's response.
 * @param {string} method - The payment method identifier to use.
 * @return {string} - The status field or error message.
 */
async function getStatus(method) { // eslint-disable-line no-unused-vars
  try {
    const request = new PaymentRequest(
        [{supportedMethods: method}],
        {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});
    const response = await request.show();
    await response.complete();
    if (!response.details.status) {
      return 'Payment handler did not specify the status.';
    }
    return response.details.status;
  } catch (e) {
    return e.message;
  }
}
