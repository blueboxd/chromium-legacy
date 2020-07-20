/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin + '/method_manifest.json';
const swSrcUrl = 'app.js';
let request;
let supportedInstruments = [];

/**
 * Install a payment app.
 * @return {string} - a message indicating whether the installation is
 *  successful.
 */
async function install() { // eslint-disable-line no-unused-vars
  info('installing');

  await navigator.serviceWorker.register(swSrcUrl);
  const registration = await navigator.serviceWorker.ready;
  if (!registration.paymentManager) {
    return 'No payment handler capability in this browser. Is' +
        'chrome://flags/#service-worker-payment-apps enabled?';
  }

  if (!registration.paymentManager.instruments) {
    return 'Payment handler is not fully implemented. ' +
        'Cannot set the instruments.';
  }
  await registration.paymentManager.instruments.set('instrument-key', {
    name: 'MaxPay',
    method: methodName,
  });
  return 'success';
}

/**
 * Uninstall the payment handler.
 * @return {string} - the message about the uninstallation result.
 */
async function uninstall() { // eslint-disable-line no-unused-vars
  info('uninstall');
  let registration = await navigator.serviceWorker.getRegistration(swSrcUrl);
  if (!registration) {
    return 'The Payment handler has not been installed yet.';
  }
  await registration.unregister();
  return 'success';
}

/**
 * Delegates handling of the provided options to the payment handler.
 * @param {Array<string>} delegations The list of payment options to delegate.
 * @return {string} The 'success' or error message.
 */
async function enableDelegations(delegations) { // eslint-disable-line no-unused-vars, max-len
  info('enableDelegations: ' + JSON.stringify(delegations));
  try {
    await navigator.serviceWorker.ready;
    let registration =
        await navigator.serviceWorker.getRegistration(swSrcUrl);
    if (!registration) {
      return 'The payment handler is not installed.';
    }
    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }
    if (!registration.paymentManager.enableDelegations) {
      return 'PaymentManager does not support enableDelegations method';
    }

    await registration.paymentManager.enableDelegations(delegations);
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Add a payment method to the payment request.
 * @param {string} method - the payment method.
 * @return {string} - a message indicating whether the operation is successful.
 */
function addSupportedMethod(method) { // eslint-disable-line no-unused-vars
  info('addSupportedMethod: ' + method);
  supportedInstruments.push({
    supportedMethods: [
      method,
    ],
  });
  return 'success';
}

/**
 * Add the payment method of this test to the payment request.
 * @return {string} - a message indicating whether the operation is successful.
 */
function addDefaultSupportedMethod() { // eslint-disable-line no-unused-vars
  return addSupportedMethod(methodName);
}

/**
 * Create a PaymentRequest.
 * @param {PaymentOptions} options - the payment options.
 * @return {string} - a message indicating whether the operation is successful.
 */
function createPaymentRequestWithOptions(options) { // eslint-disable-line no-unused-vars, max-len
  info('createPaymentRequestWithOptions: ' +
      JSON.stringify(supportedInstruments) + ', ' + JSON.stringify(options));
  const details = {
    total: {
      label: 'Donation',
      amount: {
        currency: 'USD',
        value: '55.00',
      },
    },
  };
  request = new PaymentRequest(supportedInstruments, details, options);
  return 'success';
}

/**
 * Show the payment sheet. This method is not blocking.
 * @return {string} - a message indicating whether the operation is successful.
 */
function show() { // eslint-disable-line no-unused-vars
  info('show');
  request.show().then((response) => {
    info('complete: status=' + response.details.status + ', payerName='
        + response.payerName);
    response.complete(response.details.status).then(() => {
      info('complete success');
    }).catch((e) => {
      info('complete error: ' + e);
    }).finally(() => {
      info('show finished');
    });
  }).catch((e) => {
    info('show error: ' + e);
  });
  info('show on going');
  return 'success';
}
