/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin + '/pay';
const swSrcUrl = './payment_handler_sw.js';

/**
 * Update the installation status in the widget called 'installationStatus'.
 */
async function updateStatusView() {
  const installationStatusViewId = 'installationStatus';
  const registration = await navigator.serviceWorker.getRegistration(swSrcUrl);
  if (registration) {
    document.getElementById(installationStatusViewId).innerHTML = 'installed';
  } else {
    document.getElementById(installationStatusViewId).innerHTML = 'uninstalled';
  }
}

/**
 * Insert a message to the widget called 'log'.
 * @param {Promise<string>} text - the text that is intended to be inserted
 * into the log.
 */
async function updateLogView(text) {
  const messageElement = document.getElementById('log');
  messageElement.innerHTML = (await text) + '\n' + messageElement.innerHTML;
}

/**
 * Installs the payment handler.
 * @return {string} - the message about the installation result.
 */
async function install() { // eslint-disable-line no-unused-vars
  try {
    let registration =
        await navigator.serviceWorker.getRegistration(swSrcUrl);
    if (registration) {
      return 'The payment handler is already installed.';
    }

    await navigator.serviceWorker.register(swSrcUrl);
    registration = await navigator.serviceWorker.ready;
    await updateStatusView();

    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }

    await registration.paymentManager.instruments.set('instrument-id', {
      name: 'Instrument Name',
      method: methodName,
    });
    return 'success';
  } catch (e) {
    return e.message;
  }
}

/**
 * Uninstall the payment handler.
 * @return {string} - the message about the uninstallation result.
 */
async function uninstall() { // eslint-disable-line no-unused-vars
  let registration = await navigator.serviceWorker.getRegistration(swSrcUrl);
  if (!registration) {
    return 'The Payment handler has not been installed yet.';
  }
  await registration.unregister();
  await updateStatusView();
  return 'Uninstall successfully.';
}

/**
 * Launches the payment handler.
 * @return {string} - the message about the launch result.
 */
async function launch() { // eslint-disable-line no-unused-vars
  try {
    const request = new PaymentRequest([{supportedMethods: methodName}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
    });
    const paymentResponse = await request.show();
    const status = paymentResponse.details.status;
    await updateLogView(
        `Payment App has been shown and completed with status(${status}).`);
    // status has to be either fail, success, or unknown.
    await paymentResponse.complete(status);
    return 'success';
  } catch (e) {
    return e.message;
  }
}

updateStatusView();
