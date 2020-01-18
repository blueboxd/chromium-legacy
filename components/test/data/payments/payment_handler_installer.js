/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Installs the given payment handler with the given payment methods.
 * @param {string} swUrl - The relative URL of the service worker JavaScript
 * file to install.
 * @param {string[]} methods - The list of payment methods that this service
 * worker supports.
 * @param {bool} ownScopeMethod - Whether this service worker should support its
 * own scope as a payment method.
 * @return {string} - 'success' or error message on failure.
 */
async function install(swUrl, methods, ownScopeMethod) { // eslint-disable-line no-unused-vars, max-len
  try {
    await navigator.serviceWorker.register(swUrl);
    const registration = await navigator.serviceWorker.ready;
    for (let method of methods) {
      await registration.paymentManager.instruments.set(
          'instrument-for-' + method, {name: 'Instrument Name', method});
    }
    if (ownScopeMethod) {
      await registration.paymentManager.instruments.set(
          'instrument-for-own-scope',
          {name: 'Instrument Name', method: registration.scope});
    }
    return 'success';
  } catch (e) {
    return e.message;
  }
}
