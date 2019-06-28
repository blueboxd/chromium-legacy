/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

/**
 * Helper function that launches the PaymentRequest UI with the specified
 * payment methods.
 *
 * @param {!Array<!Object>} methods: Payment methods data for PaymentRequest
 *     constructor.
 */
function testPaymentMethods(methods) {
  try {
    new PaymentRequest(
        methods,
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}})
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
              })
              .catch(function(error) {
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI with Bob Pay and credit cards as payment
 * methods.
 */
function buy() {  // eslint-disable-line no-unused-vars
  testPaymentMethods([
      {supportedMethods: 'https://bobpay.com'},
      {
        supportedMethods: 'basic-card',
        data: {supportedNetworks: ['visa', 'mastercard']},
      },
  ]);
}

/**
 * Launches the PaymentRequest UI with kylepay.com and basic-card as payment
 * methods. kylepay.com hosts an installable payment app.
 */
function testInstallableAppAndCard() {  // eslint-disable-line no-unused-vars
  testPaymentMethods([
      {supportedMethods: 'https://kylepay.com/webpay'},
      {supportedMethods: 'basic-card'},
  ]);
}
